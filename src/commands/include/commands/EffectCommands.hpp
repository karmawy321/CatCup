#pragma once

#include "commands/Command.hpp"
#include "core/Model.hpp"

#include <map>
#include <memory>
#include <string>

namespace editor::commands {

std::unique_ptr<ICommand> makeAddEffectCommand(const core::Id& clipId, core::Effect effect);
std::unique_ptr<ICommand> makeRemoveEffectCommand(const core::Id& clipId, size_t effectIndex);
std::unique_ptr<ICommand> makeUpdateEffectCommand(const core::Id& clipId, size_t effectIndex,
                                                  std::map<std::string, double> params,
                                                  std::map<std::string, std::string> strParams);
std::unique_ptr<ICommand> makeSetClipSpeedCommand(const core::Id& clipId, core::Rational newSpeed,
                                                  bool ripple = false);

} // namespace editor::commands
