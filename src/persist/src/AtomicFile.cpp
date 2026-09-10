#include "persist/AtomicFile.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace editor::persist {
namespace fs = std::filesystem;

core::Result<std::string> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return core::Result<std::string>::fail("cannot open file for reading: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (in.bad()) {
        return core::Result<std::string>::fail("failed while reading file: " + path);
    }
    return core::Result<std::string>::ok(ss.str());
}

core::Result<void> atomicWriteFile(const std::string& path, const std::string& bytes) {
    try {
        const fs::path target(path);
        if (target.has_parent_path()) {
            fs::create_directories(target.parent_path());
        }
        const fs::path tmp = target.string() + ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) {
                return core::Result<void>::fail("cannot open temp file for writing: " + tmp.string());
            }
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            out.flush();
            if (!out) {
                return core::Result<void>::fail("failed while writing temp file: " + tmp.string());
            }
        }
        // Keep the previous good file as a recovery journal.
        std::error_code ec;
        if (fs::exists(target, ec)) {
            const fs::path journal = target.string() + ".journal";
            fs::remove(journal, ec);
            fs::rename(target, journal, ec);
            if (ec) {
                fs::remove(tmp, ec);
                return core::Result<void>::fail("cannot rotate journal file: " + ec.message());
            }
        }
        fs::rename(tmp, target, ec);
        if (ec) {
            return core::Result<void>::fail("cannot rename temp file into place: " + ec.message());
        }
        return core::Result<void>::ok();
    } catch (const std::exception& e) {
        return core::Result<void>::fail(std::string("atomic write failed: ") + e.what());
    }
}

} // namespace editor::persist
