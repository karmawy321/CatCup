#pragma once

#include "commands/Command.hpp"
#include "core/Model.hpp"

namespace editor::commands {

std::unique_ptr<ICommand> makeAddTransitionCommand(core::Transition transition);
std::unique_ptr<ICommand> makeRemoveTransitionCommand(const core::Id& transitionId);
std::unique_ptr<ICommand> makeUpdateTransitionCommand(const core::Id& transitionId,
                                                      core::Rational newDuration,
                                                      core::TransitionAlignment newAlign,
                                                      std::string newType,
                                                      std::string newEasing);

} // namespace editor::commands
