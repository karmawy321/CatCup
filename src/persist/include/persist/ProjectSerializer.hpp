#pragma once

// Project <-> JSON document conversion plus file-level save/load.
// Rational time is stored as {num, den} integer pairs — never as doubles.

#include "core/Model.hpp"
#include "core/Result.hpp"
#include "persist/Json.hpp"

#include <string>

namespace editor::persist {

core::Result<JsonValue> projectToJson(const core::Project& project);
core::Result<core::Project> projectFromJson(const JsonValue& root);

core::Result<void> saveProject(const core::Project& project, const std::string& path);
core::Result<core::Project> loadProject(const std::string& path);

// Asset-path policy (BUILD-PLAN "relative asset references; relink support"):
//   on disk   — paths under the project directory are stored relative
//               ("media/shot.mp4"); anything else stays absolute.
//   in memory — loadProject() always resolves to absolute, so decoders,
//               the exporter, and thumbnails work regardless of CWD.
// Lexical only (no filesystem probing): missing files still load, and the
// UI can offer relink instead of failing the whole project.
std::string resolveAssetPath(const std::string& projectFile, const std::string& stored);
std::string storeAssetPath(const std::string& projectFile, const std::string& absolute);

} // namespace editor::persist
