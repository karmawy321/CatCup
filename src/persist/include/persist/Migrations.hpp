#pragma once

// Schema migrations. Documents carry an integer `schemaVersion`. The loader
// migrates step-by-step to core::kCurrentSchemaVersion. Unknown future
// majors are rejected — never silently loaded.

#include "core/Result.hpp"
#include "persist/Json.hpp"

namespace editor::persist {

/// Migrate a parsed document root in place. Returns the resulting version.
core::Result<int> migrateDocument(JsonValue& root);

} // namespace editor::persist
