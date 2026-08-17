#include "doctest.h"
#include "../connection.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "nlohmann/json.hpp"

namespace nl = nlohmann;

namespace {

// A per-process temp directory, so concurrent test runs do not collide.
std::string test_dir() {
    return "/tmp/mx-kernel-test-" + std::to_string(getpid());
}

// Point JUPYTER_RUNTIME_DIR at a private directory for the duration of a test.
struct scoped_runtime_dir {
    std::string dir;
    std::string previous;
    bool had_previous = false;

    scoped_runtime_dir() : dir(test_dir()) {
        if (const char* p = std::getenv("JUPYTER_RUNTIME_DIR")) {
            previous = p;
            had_previous = true;
        }
        setenv("JUPYTER_RUNTIME_DIR", dir.c_str(), 1);
    }

    ~scoped_runtime_dir() {
        if (had_previous) {
            setenv("JUPYTER_RUNTIME_DIR", previous.c_str(), 1);
        } else {
            unsetenv("JUPYTER_RUNTIME_DIR");
        }
        rmdir(dir.c_str());
    }
};

xeus::xconfiguration config_with_known_ports() {
    auto config = mx::create_kernel_configuration();
    config.m_shell_port = "12345";
    config.m_control_port = "12346";
    config.m_stdin_port = "12347";
    config.m_iopub_port = "12348";
    config.m_hb_port = "12349";
    return config;
}

} // namespace

TEST_CASE("generate_random_key") {
    SUBCASE("produces 64-character string") {
        std::string key = mx::generate_random_key();
        CHECK(key.size() == 64);
    }

    SUBCASE("only contains hex characters") {
        std::string key = mx::generate_random_key();
        for (char c : key) {
            bool is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
            CHECK(is_hex);
        }
    }

    SUBCASE("generates unique keys") {
        std::set<std::string> keys;
        for (int i = 0; i < 100; ++i) {
            keys.insert(mx::generate_random_key());
        }
        // All 100 should be distinct (collision probability is negligible)
        CHECK(keys.size() == 100);
    }
}

TEST_CASE("create_kernel_configuration") {
    auto config = mx::create_kernel_configuration();

    SUBCASE("default transport and ip") {
        CHECK(config.m_transport == "tcp");
        CHECK(config.m_ip == "127.0.0.1");
    }

    SUBCASE("auto-assign ports") {
        CHECK(config.m_control_port == "0");
        CHECK(config.m_shell_port == "0");
        CHECK(config.m_stdin_port == "0");
        CHECK(config.m_iopub_port == "0");
        CHECK(config.m_hb_port == "0");
    }

    SUBCASE("hmac signature scheme") {
        CHECK(config.m_signature_scheme == "hmac-sha256");
    }

    SUBCASE("key is valid") {
        CHECK(config.m_key.size() == 64);
    }
}

TEST_CASE("sanitize_kernel_name") {
    SUBCASE("keeps safe characters") {
        CHECK(mx::sanitize_kernel_name("my-kernel_1.2") == "my-kernel_1.2");
    }

    SUBCASE("strips path separators") {
        CHECK(mx::sanitize_kernel_name("a/b") == "ab");
        CHECK(mx::sanitize_kernel_name("a\\b") == "ab");
    }

    SUBCASE("cannot escape the runtime directory") {
        // "../../etc/passwd" must not survive as a traversal.
        const std::string cleaned = mx::sanitize_kernel_name("../../etc/passwd");
        CHECK(cleaned.find("..") == std::string::npos);
        CHECK(cleaned.find('/') == std::string::npos);
        CHECK(cleaned == "etcpasswd");
    }

    SUBCASE("rejects names that are only dots") {
        CHECK(mx::sanitize_kernel_name(".") == "");
        CHECK(mx::sanitize_kernel_name("..") == "");
    }

    SUBCASE("strips leading dots so the file is not hidden") {
        CHECK(mx::sanitize_kernel_name(".hidden") == "hidden");
    }

    SUBCASE("drops shell metacharacters and spaces") {
        CHECK(mx::sanitize_kernel_name("a b; rm -rf $HOME") == "abrm-rfHOME");
    }

    SUBCASE("empty stays empty") {
        CHECK(mx::sanitize_kernel_name("") == "");
    }
}

TEST_CASE("write_connection_file") {
    scoped_runtime_dir runtime;
    auto config = config_with_known_ports();

    std::string path = mx::write_connection_file(config, "test-kernel");

    SUBCASE("creates the runtime directory if it is missing") {
        struct stat st;
        REQUIRE(stat(runtime.dir.c_str(), &st) == 0);
        CHECK((st.st_mode & S_IFMT) == S_IFDIR);
    }

    SUBCASE("file path includes kernel name") {
        CHECK(path.find("kernel-test-kernel.json") != std::string::npos);
    }

    SUBCASE("file contains valid JSON") {
        std::ifstream f(path);
        REQUIRE(f.is_open());
        nl::json j = nl::json::parse(f);

        CHECK(j["transport"] == "tcp");
        CHECK(j["ip"] == "127.0.0.1");
        CHECK(j["shell_port"] == 12345);
        CHECK(j["control_port"] == 12346);
        CHECK(j["stdin_port"] == 12347);
        CHECK(j["iopub_port"] == 12348);
        CHECK(j["hb_port"] == 12349);
        CHECK(j["signature_scheme"] == "hmac-sha256");
        CHECK(j["key"].get<std::string>().size() == 64);
    }

    SUBCASE("file is readable only by its owner") {
        // It carries the HMAC key: any other local user who can read it can
        // connect to the kernel and drive the patch.
        struct stat st;
        REQUIRE(stat(path.c_str(), &st) == 0);
        const mode_t mode = st.st_mode & 0777;
        CHECK(mode == 0600);
        CHECK((mode & S_IRGRP) == 0);
        CHECK((mode & S_IROTH) == 0);
    }

    std::remove(path.c_str());
}

TEST_CASE("write_connection_file tightens permissions on an existing file") {
    scoped_runtime_dir runtime;
    REQUIRE(mx::make_directories(runtime.dir));

    // Pre-create the target world-readable, as a previous version would leave it.
    const std::string path = runtime.dir + "/kernel-stale.json";
    {
        std::ofstream f(path);
        f << "{}";
    }
    chmod(path.c_str(), 0644);

    auto config = config_with_known_ports();
    const std::string written = mx::write_connection_file(config, "stale");
    CHECK(written == path);

    struct stat st;
    REQUIRE(stat(path.c_str(), &st) == 0);
    CHECK((st.st_mode & 0777) == 0600);

    std::remove(path.c_str());
}

TEST_CASE("write_connection_file rejects an unusable kernel name") {
    scoped_runtime_dir runtime;
    auto config = config_with_known_ports();

    CHECK_THROWS_AS(mx::write_connection_file(config, ".."), std::runtime_error);
}

TEST_CASE("write_connection_file reports unparseable ports") {
    scoped_runtime_dir runtime;
    auto config = mx::create_kernel_configuration();
    config.m_shell_port = "not-a-port";

    CHECK_THROWS_AS(mx::write_connection_file(config, "bad-ports"),
                    std::runtime_error);
}

TEST_CASE("make_directories") {
    const std::string root = test_dir() + "-nested";
    const std::string deep = root + "/a/b/c";

    REQUIRE(mx::make_directories(deep));

    struct stat st;
    CHECK(stat(deep.c_str(), &st) == 0);
    CHECK((st.st_mode & S_IFMT) == S_IFDIR);

    SUBCASE("is idempotent") {
        CHECK(mx::make_directories(deep));
    }

    SUBCASE("empty path fails") {
        CHECK(!mx::make_directories(""));
    }

    rmdir((root + "/a/b/c").c_str());
    rmdir((root + "/a/b").c_str());
    rmdir((root + "/a").c_str());
    rmdir(root.c_str());
}
