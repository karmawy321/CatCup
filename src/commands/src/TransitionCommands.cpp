#include "commands/TransitionCommands.hpp"

namespace editor::commands {
namespace {

class AddTransitionCommand final : public ICommand {
public:
    explicit AddTransitionCommand(core::Transition transition)
        : transition_(std::move(transition)) {}

    std::string label() const override { return "Add transition " + transition_.type; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        if (seq->findTransition(transition_.id) != nullptr) {
            error = "transition id already exists: " + transition_.id;
            return false;
        }
        core::Track* tr = seq->findTrack(transition_.trackId);
        if (tr == nullptr) {
            error = "track not found: " + transition_.trackId;
            return false;
        }
        if (tr->locked) {
            error = "track is locked: " + tr->name;
            return false;
        }
        if (seq->findClip(transition_.fromClipId) == nullptr) {
            error = "fromClip not found: " + transition_.fromClipId;
            return false;
        }
        if (seq->findClip(transition_.toClipId) == nullptr) {
            error = "toClip not found: " + transition_.toClipId;
            return false;
        }
        seq->transitions.push_back(transition_);
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            seq->removeTransition(transition_.id);
        }
    }

private:
    core::Transition transition_;
};

class RemoveTransitionCommand final : public ICommand {
public:
    explicit RemoveTransitionCommand(core::Id transitionId)
        : transitionId_(std::move(transitionId)) {}

    std::string label() const override { return "Remove transition"; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        const core::Transition* tr = seq->findTransition(transitionId_);
        if (tr == nullptr) {
            error = "transition not found: " + transitionId_;
            return false;
        }
        saved_ = *tr;
        seq->removeTransition(transitionId_);
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            seq->transitions.push_back(saved_);
        }
    }

private:
    core::Id transitionId_;
    core::Transition saved_;
};

class UpdateTransitionCommand final : public ICommand {
public:
    UpdateTransitionCommand(core::Id transitionId, core::Rational newDuration,
                            core::TransitionAlignment newAlign, std::string newType,
                            std::string newEasing)
        : transitionId_(std::move(transitionId)),
          newDuration_(newDuration),
          newAlign_(newAlign),
          newType_(std::move(newType)),
          newEasing_(std::move(newEasing)) {}

    std::string label() const override { return "Update transition"; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        core::Transition* tr = seq->findTransition(transitionId_);
        if (tr == nullptr) {
            error = "transition not found: " + transitionId_;
            return false;
        }
        oldDuration_ = tr->duration;
        oldAlign_ = tr->alignment;
        oldType_ = tr->type;
        oldEasing_ = tr->easing;

        tr->duration = newDuration_;
        tr->alignment = newAlign_;
        if (!newType_.empty()) {
            tr->type = newType_;
        }
        if (!newEasing_.empty()) {
            tr->easing = newEasing_;
        }
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Transition* tr = seq->findTransition(transitionId_)) {
                tr->duration = oldDuration_;
                tr->alignment = oldAlign_;
                tr->type = oldType_;
                tr->easing = oldEasing_;
            }
        }
    }

private:
    core::Id transitionId_;
    core::Rational newDuration_;
    core::TransitionAlignment newAlign_;
    std::string newType_;
    std::string newEasing_;

    core::Rational oldDuration_{1, 1};
    core::TransitionAlignment oldAlign_{core::TransitionAlignment::CenterOnCut};
    std::string oldType_;
    std::string oldEasing_;
};

} // namespace

std::unique_ptr<ICommand> makeAddTransitionCommand(core::Transition transition) {
    return std::make_unique<AddTransitionCommand>(std::move(transition));
}

std::unique_ptr<ICommand> makeRemoveTransitionCommand(const core::Id& transitionId) {
    return std::make_unique<RemoveTransitionCommand>(transitionId);
}

std::unique_ptr<ICommand> makeUpdateTransitionCommand(const core::Id& transitionId,
                                                      core::Rational newDuration,
                                                      core::TransitionAlignment newAlign,
                                                      std::string newType,
                                                      std::string newEasing) {
    return std::make_unique<UpdateTransitionCommand>(transitionId, newDuration, newAlign,
                                                     std::move(newType), std::move(newEasing));
}

} // namespace editor::commands
