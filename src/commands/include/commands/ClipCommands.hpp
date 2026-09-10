#pragma once

// Concrete timeline edits used by the Stage 1 acceptance flow:
// place / remove / move / trim / split a clip, plus title insertion.

#include "commands/Command.hpp"

namespace editor::commands {

std::unique_ptr<ICommand> makeAddClipCommand(const core::Id& trackId, core::Clip clip);
std::unique_ptr<ICommand> makeRemoveClipCommand(const core::Id& trackId, const core::Id& clipId);
std::unique_ptr<ICommand> makeMoveClipCommand(const core::Id& clipId, core::Rational newStart);
std::unique_ptr<ICommand> makeTrimClipCommand(const core::Id& clipId, core::Rational newIn,
                                              core::Rational newOut, core::Rational newStart);
/// Split clipId at sequence time `at`. rightId is the stable id for the new
/// right-hand clip (caller generates it so ids stay deterministic in tests).
std::unique_ptr<ICommand> makeSplitClipCommand(const core::Id& clipId, core::Rational at,
                                               core::Id rightId);
/// Replace a clip's transform (inspector numeric edits stay undoable).
std::unique_ptr<ICommand> makeSetTransformCommand(const core::Id& clipId,
                                                  core::Transform transform);
/// Replace a title clip's text payload (text, font, size stay undoable).
std::unique_ptr<ICommand> makeSetTextCommand(const core::Id& clipId, std::string text,
                                             std::string fontFamily, double fontSizePt);

} // namespace editor::commands
