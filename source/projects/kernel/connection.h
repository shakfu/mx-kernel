#pragma once

#include <string>
#include "xeus/xkernel_configuration.hpp"

namespace mx {

// Generate a 64-character hex key using a CSPRNG.
// Uses arc4random_buf on macOS, falls back to std::random_device elsewhere.
std::string generate_random_key();

// Create a default kernel configuration with auto-assigned ports and a fresh key.
xeus::xconfiguration create_kernel_configuration();

// Write a Jupyter-compatible connection file to the runtime directory.
// Returns the absolute path to the written file.
// Throws std::runtime_error on failure.
std::string write_connection_file(const xeus::xconfiguration& config,
                                  const std::string& kernel_name);

} // namespace mx
