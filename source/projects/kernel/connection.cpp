#include "connection.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <stdlib.h> // arc4random_buf
#endif

#include "nlohmann/json.hpp"

namespace nl = nlohmann;

// std::filesystem is deliberately avoided: the Max SDK pins the macOS
// deployment target to 10.11 and <filesystem> is unavailable before 10.15.

namespace mx {

namespace {

// Fill buf with cryptographically secure random bytes, or throw.
void secure_random_bytes(unsigned char* buf, size_t len) {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    arc4random_buf(buf, len);
#elif defined(_WIN32)
    if (BCryptGenRandom(nullptr, buf, static_cast<ULONG>(len),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("BCryptGenRandom failed");
    }
#else
    std::FILE* f = std::fopen("/dev/urandom", "rb");
    if (!f) {
        throw std::runtime_error("cannot open /dev/urandom");
    }
    const size_t got = std::fread(buf, 1, len, f);
    std::fclose(f);
    if (got != len) {
        throw std::runtime_error("short read from /dev/urandom");
    }
#endif
}

bool is_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return (st.st_mode & S_IFMT) == S_IFDIR;
}

bool make_one_directory(const std::string& path) {
#if defined(_WIN32)
    return _mkdir(path.c_str()) == 0 || is_directory(path);
#else
    return mkdir(path.c_str(), 0777) == 0 || is_directory(path);
#endif
}

} // namespace

bool make_directories(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (is_directory(path)) {
        return true;
    }

    // Create each component in turn, tolerating ones that already exist.
    std::string partial;
    partial.reserve(path.size());

    for (size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        partial += c;

        const bool is_sep = (c == '/')
#if defined(_WIN32)
                         || (c == '\\')
#endif
            ;
        if (!is_sep || partial == "/" || i == 0) {
            continue;
        }
        if (!make_one_directory(partial)) {
            return false;
        }
    }

    return make_one_directory(path);
}

std::string generate_random_key() {
    const char* hex_chars = "0123456789abcdef";
    unsigned char buf[32];

    secure_random_bytes(buf, sizeof(buf));

    std::string key;
    key.reserve(64);
    for (unsigned char b : buf) {
        key += hex_chars[(b >> 4) & 0x0f];
        key += hex_chars[b & 0x0f];
    }
    return key;
}

xeus::xconfiguration create_kernel_configuration() {
    xeus::xconfiguration config;
    config.m_transport = "tcp";
    config.m_ip = "127.0.0.1";
    config.m_control_port = "0";
    config.m_shell_port = "0";
    config.m_stdin_port = "0";
    config.m_iopub_port = "0";
    config.m_hb_port = "0";
    config.m_signature_scheme = "hmac-sha256";
    config.m_key = generate_random_key();
    return config;
}

std::string sanitize_kernel_name(const std::string& name) {
    std::string out;
    out.reserve(name.size());

    for (char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                     || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (ok) {
            out += c;
        }
    }

    // A leading dot would hide the file, and "." / ".." are path traversal.
    const size_t first = out.find_first_not_of('.');
    if (first == std::string::npos) {
        return "";
    }
    return out.substr(first);
}

std::string write_connection_file(const xeus::xconfiguration& config,
                                  const std::string& kernel_name) {
    const char* runtime_dir = std::getenv("JUPYTER_RUNTIME_DIR");
    std::string base_dir;

    if (runtime_dir) {
        base_dir = runtime_dir;
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            base_dir = std::string(home) + "/.local/share/jupyter/runtime";
        } else {
            base_dir = "/tmp";
        }
    }

    const std::string safe_name = sanitize_kernel_name(kernel_name);
    if (safe_name.empty()) {
        throw std::runtime_error("Invalid kernel name: " + kernel_name);
    }

    if (!make_directories(base_dir)) {
        throw std::runtime_error("Failed to create runtime directory: " + base_dir);
    }

    const std::string filename = base_dir + "/kernel-" + safe_name + ".json";

    nl::json connection_info;
    connection_info["transport"] = config.m_transport;
    connection_info["ip"] = config.m_ip;

    try {
        connection_info["control_port"] = std::stoi(config.m_control_port);
        connection_info["shell_port"] = std::stoi(config.m_shell_port);
        connection_info["stdin_port"] = std::stoi(config.m_stdin_port);
        connection_info["iopub_port"] = std::stoi(config.m_iopub_port);
        connection_info["hb_port"] = std::stoi(config.m_hb_port);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to parse ports: ") + e.what() +
                                 " (shell=" + config.m_shell_port +
                                 ", control=" + config.m_control_port + ")");
    }

    connection_info["signature_scheme"] = config.m_signature_scheme;
    connection_info["key"] = config.m_key;

    const std::string payload = connection_info.dump(2);

    // The file carries the HMAC key, so it must never be readable by other
    // local users. Creating it 0600 up front is what makes that airtight:
    // writing first and tightening afterwards would leave a window in which
    // the key is world-readable.
#if defined(_WIN32)
    std::FILE* f = std::fopen(filename.c_str(), "wb");
    if (!f) {
        throw std::runtime_error("Failed to open connection file for writing: " +
                                 filename);
    }
    _chmod(filename.c_str(), _S_IREAD | _S_IWRITE);
#else
    const int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        throw std::runtime_error("Failed to open connection file for writing: " +
                                 filename);
    }

    // A pre-existing file keeps its old mode, so tighten it explicitly.
    if (fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
        close(fd);
        std::remove(filename.c_str());
        throw std::runtime_error("Failed to restrict permissions on connection file: " +
                                 filename);
    }

    std::FILE* f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        std::remove(filename.c_str());
        throw std::runtime_error("Failed to open connection file for writing: " +
                                 filename);
    }
#endif

    const size_t written = std::fwrite(payload.data(), 1, payload.size(), f);
    const bool flushed = (std::fclose(f) == 0);

    if (written != payload.size() || !flushed) {
        std::remove(filename.c_str());
        throw std::runtime_error("Failed to write connection file: " + filename);
    }

    return filename;
}

} // namespace mx
