#pragma once

#include "core/Model.hpp"
#include "core/Result.hpp"

#include <string>

namespace editor::persist {

class CapCutDraftBridge {
public:
    /// Imports a CapCut draft_content.json into a native CatCup Project.
    static core::Result<core::Project> importDraft(const std::string& draftJsonPath);

    /// Exports a native CatCup Project to CapCut draft_content.json format.
    static core::Result<void> exportDraft(const core::Project& project,
                                          const std::string& draftJsonPath);
};

} // namespace editor::persist
