#include "connection.h"

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#ifdef __APPLE__
#include <stdlib.h> // arc4random_buf
#else
#include <random>
#endif

#include "nlohmann/json.hpp"

namespace nl = nlohmann;

namespace mx {

std::string generate_random_key() {
    const char* hex_chars = "0123456789abcdef";
    unsigned char buf[32];

#ifdef __APPLE__
    arc4random_buf(buf, sizeof(buf));
#else
    std::random_device rd;
    for (auto& b : buf) {
        b = static_cast<unsigned char>(rd());
    }
#endif

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

    std::string filename = base_dir + "/kernel-" + kernel_name + ".json";

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

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open connection file for writing: " + filename);
    }

    file << connection_info.dump(2);
    file.close();

    if (file.fail()) {
        throw std::runtime_error("Failed to write connection file: " + filename);
    }

    return filename;
}

} // namespace mx
