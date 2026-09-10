#pragma once

// Atomic file writes: temp file in the same directory + flush + rename, with
// a `.journal` backup of the previous good file for crash recovery.

#include "core/Result.hpp"

#include <string>

namespace editor::persist {

core::Result<void> atomicWriteFile(const std::string& path, const std::string& bytes);
core::Result<std::string> readFile(const std::string& path);

} // namespace editor::persist
