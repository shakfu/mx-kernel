#pragma once

#include <string>
#include "xeus/xkernel_configuration.hpp"

namespace mx {

// Generate a 64-character hex key using a CSPRNG.
// arc4random_buf on Apple/BSD, BCryptGenRandom on Windows, /dev/urandom
// elsewhere. std::random_device is not used: it is permitted to be
// deterministic, and historically was on some toolchains.
std::string generate_random_key();

// Create a default kernel configuration with auto-assigned ports and a fresh key.
xeus::xconfiguration create_kernel_configuration();

// Create a directory and any missing parents. Returns true if the directory
// exists on return. (A local mkdir -p: std::filesystem is unavailable at the
// macOS deployment target the Max SDK pins.)
bool make_directories(const std::string& path);

// Reduce a user-supplied kernel name to characters that are safe in a filename.
// Anything outside [A-Za-z0-9._-] is dropped, and leading dots are stripped, so
// a name cannot escape the runtime directory. Returns "" if nothing survives.
std::string sanitize_kernel_name(const std::string& name);

// Write a Jupyter-compatible connection file to the runtime directory, creating
// the directory if needed. The file contains the HMAC key, so it is created
// with owner-only permissions (0600), matching what Jupyter itself does.
// Returns the absolute path to the written file.
// Throws std::runtime_error on failure.
std::string write_connection_file(const xeus::xconfiguration& config,
                                  const std::string& kernel_name);

} // namespace mx
