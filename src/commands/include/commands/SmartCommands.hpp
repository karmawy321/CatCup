#pragma once

#include "ai/AudioAnalysis.hpp"
#include "ai/CaptionEngine.hpp"
#include "commands/Command.hpp"
#include "core/Model.hpp"

#include <memory>
#include <vector>

namespace editor::commands {

/// Cut out silent portions of a clip and ripple-close the resulting gaps.
std::unique_ptr<ICommand> makeSilenceCutCommand(
    const core::Id& trackId,
    const core::Id& clipId,
    const std::vector<ai::SilenceInterval>& silences
);

/// Split a clip at multiple detected scene cut points.
std::unique_ptr<ICommand> makeSceneSplitCommand(
    const core::Id& clipId,
    const std::vector<core::Rational>& cutPoints
);

/// Add generated caption clips onto a text/subtitle track.
std::unique_ptr<ICommand> makeAddCaptionsCommand(
    const core::Id& trackId,
    const std::vector<ai::CaptionCue>& cues
);

} // namespace editor::commands
