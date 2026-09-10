#include "commands/Command.hpp"

namespace editor::commands {

UndoStack::UndoStack(std::size_t capacity, core::ChangeBus* bus)
    : capacity_(capacity == 0 ? 1 : capacity), bus_(bus) {}

bool UndoStack::execute(std::unique_ptr<ICommand> cmd, core::Project& project,
                        std::string& error) {
    if (!cmd) {
        error = "null command";
        return false;
    }
    if (!cmd->execute(project, error)) {
        return false;
    }
    undo_.push_back(std::move(cmd));
    if (undo_.size() > capacity_) {
        undo_.erase(undo_.begin());
    }
    redo_.clear();
    return true;
}

bool UndoStack::undo(core::Project& project) {
    if (undo_.empty()) {
        return false;
    }
    auto cmd = std::move(undo_.back());
    undo_.pop_back();
    cmd->undo(project);
    redo_.push_back(std::move(cmd));
    if (bus_ != nullptr) {
        bus_->publish(core::ProjectEvent{core::ProjectEvent::Type::UndoPerformed, {}});
    }
    return true;
}

bool UndoStack::redo(core::Project& project, std::string& error) {
    if (redo_.empty()) {
        return false;
    }
    auto cmd = std::move(redo_.back());
    redo_.pop_back();
    if (!cmd->execute(project, error)) {
        return false;
    }
    undo_.push_back(std::move(cmd));
    if (bus_ != nullptr) {
        bus_->publish(core::ProjectEvent{core::ProjectEvent::Type::RedoPerformed, {}});
    }
    return true;
}

void UndoStack::clear() {
    undo_.clear();
    redo_.clear();
}

} // namespace editor::commands
