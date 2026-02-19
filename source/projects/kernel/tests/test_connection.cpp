#include "doctest.h"
#include "../connection.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <string>

#include "nlohmann/json.hpp"

namespace nl = nlohmann;

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

TEST_CASE("write_connection_file") {
    // Use a temp directory to avoid polluting the real runtime dir
    std::string tmpdir = "/tmp/mx-kernel-test";
    std::system(("mkdir -p " + tmpdir).c_str());

    // Override JUPYTER_RUNTIME_DIR
    setenv("JUPYTER_RUNTIME_DIR", tmpdir.c_str(), 1);

    auto config = mx::create_kernel_configuration();
    // Set ports to known values for testing
    config.m_shell_port = "12345";
    config.m_control_port = "12346";
    config.m_stdin_port = "12347";
    config.m_iopub_port = "12348";
    config.m_hb_port = "12349";

    std::string path = mx::write_connection_file(config, "test-kernel");

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

    // Cleanup
    std::remove(path.c_str());
    unsetenv("JUPYTER_RUNTIME_DIR");
    std::system(("rmdir " + tmpdir + " 2>/dev/null || true").c_str());
}
