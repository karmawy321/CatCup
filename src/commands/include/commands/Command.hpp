#pragma once

// Command pattern: every persistent edit is an ICommand so undo, redo,
// autosave, and the future assistant share one code path. Commands validate
// preconditions in execute() and return false (leaving the project untouched)
// when they do not apply — e.g. operating on a locked track.

#include "core/ChangeBus.hpp"
#include "core/Model.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace editor::commands {

class ICommand {
public:
    virtual ~ICommand() = default;
    [[nodiscard]] virtual std::string label() const = 0;
    /// Apply. Returns false + error when preconditions fail (no mutation).
    virtual bool execute(core::Project& project, std::string& error) = 0;
    virtual void undo(core::Project& project) = 0;
};

class UndoStack {
public:
    explicit UndoStack(std::size_t capacity = 100, core::ChangeBus* bus = nullptr);

    bool execute(std::unique_ptr<ICommand> cmd, core::Project& project, std::string& error);
    [[nodiscard]] bool canUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !redo_.empty(); }
    bool undo(core::Project& project);
    bool redo(core::Project& project, std::string& error);
    void clear();
    [[nodiscard]] std::size_t undoSize() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redoSize() const noexcept { return redo_.size(); }

private:
    std::size_t capacity_;
    core::ChangeBus* bus_; // optional, not owned
    std::vector<std::unique_ptr<ICommand>> undo_;
    std::vector<std::unique_ptr<ICommand>> redo_;
};

} // namespace editor::commands
