#include "commands/EffectCommands.hpp"

#include <algorithm>

namespace editor::commands {

namespace {

class AddEffectCommand final : public ICommand {
public:
    AddEffectCommand(core::Id clipId, core::Effect effect)
        : clipId_(std::move(clipId)), effect_(std::move(effect)) {}

    std::string label() const override { return "Add Effect"; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        core::Clip* clip = seq->findClip(clipId_);
        if (clip == nullptr) {
            error = "clip not found: " + clipId_;
            return false;
        }
        clip->effects.push_back(effect_);
        return true;
    }

    void undo(core::Project& project) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) return;
        core::Clip* clip = seq->findClip(clipId_);
        if (clip == nullptr || clip->effects.empty()) return;
        clip->effects.pop_back();
    }

private:
    core::Id clipId_;
    core::Effect effect_;
};

class RemoveEffectCommand final : public ICommand {
public:
    RemoveEffectCommand(core::Id clipId, size_t effectIndex)
        : clipId_(std::move(clipId)), index_(effectIndex) {}

    std::string label() const override { return "Remove Effect"; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        core::Clip* clip = seq->findClip(clipId_);
        if (clip == nullptr) {
            error = "clip not found: " + clipId_;
            return false;
        }
        if (index_ >= clip->effects.size()) {
            error = "effect index out of range";
            return false;
        }
        savedEffect_ = clip->effects[index_];
        clip->effects.erase(clip->effects.begin() + static_cast<ptrdiff_t>(index_));
        return true;
    }

    void undo(core::Project& project) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) return;
        core::Clip* clip = seq->findClip(clipId_);
        if (clip == nullptr) return;
        if (index_ <= clip->effects.size()) {
            clip->effects.insert(clip->effects.begin() + static_cast<ptrdiff_t>(index_), savedEffect_);
        }
    }

private:
    core::Id clipId_;
    size_t index_;
    core::Effect savedEffect_;
};

class UpdateEffectCommand final : public ICommand {
public:
    UpdateEffectCommand(core::Id clipId, size_t effectIndex,
                        std::map<std::string, double> params,
                        std::map<std::string, std::string> strParams)
        : clipId_(std::move(clipId)), index_(effectIndex),
          newParams_(std::move(params)), newStrParams_(std::move(strParams)) {}

    std::string label() const override { return "Update Effect"; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        core::Clip* clip = seq->findClip(clipId_);
        if (clip == nullptr) {
            error = "clip not found: " + clipId_;
            return false;
        }
        if (index_ >= clip->effects.size()) {
            error = "effect index out of range";
            return false;
        }
        oldParams_ = clip->effects[index_].params;
        oldStrParams_ = clip->effects[index_].strParams;

        for (const auto& [k, v] : newParams_) {
            clip->effects[index_].params[k] = v;
        }
        for (const auto& [k, v] : newStrParams_) {
            clip->effects[index_].strParams[k] = v;
        }
        return true;
    }

    void undo(core::Project& project) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) return;
        core::Clip* clip = seq->findClip(clipId_);
        if (clip == nullptr || index_ >= clip->effects.size()) return;
        clip->effects[index_].params = oldParams_;
        clip->effects[index_].strParams = oldStrParams_;
    }

private:
    core::Id clipId_;
    size_t index_;
    std::map<std::string, double> newParams_;
    std::map<std::string, std::string> newStrParams_;
    std::map<std::string, double> oldParams_;
    std::map<std::string, std::string> oldStrParams_;
};

class SetClipSpeedCommand final : public ICommand {
public:
    SetClipSpeedCommand(core::Id clipId, core::Rational newSpeed, bool ripple)
        : clipId_(std::move(clipId)), newSpeed_(newSpeed), ripple_(ripple) {}

    std::string label() const override { return "Set Clip Speed"; }

    bool execute(core::Project& project, std::string& error) override {
        if (newSpeed_.num() <= 0) {
            error = "speed must be positive";
            return false;
        }
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        core::Clip* clip = seq->findClip(clipId_);
        if (clip == nullptr) {
            error = "clip not found: " + clipId_;
            return false;
        }
        oldSpeed_ = clip->speed;
        const core::Rational oldDur = clip->seqDuration();
        clip->speed = newSpeed_;
        const core::Rational newDur = clip->seqDuration();
        const core::Rational durDelta = newDur - oldDur;

        downstreamShifts_.clear();
        if (ripple_ && durDelta.num() != 0) {
            for (const auto& track : seq->tracks) {
                bool found = false;
                for (const auto& cid : track.clipIds) {
                    if (cid == clipId_) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    for (const auto& cid : track.clipIds) {
                        if (cid != clipId_) {
                            core::Clip* other = seq->findClip(cid);
                            if (other != nullptr && other->seqStart >= clip->seqStart) {
                                downstreamShifts_.push_back({cid, other->seqStart});
                                other->seqStart = other->seqStart + durDelta;
                            }
                        }
                    }
                    break;
                }
            }
        }
        return true;
    }

    void undo(core::Project& project) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) return;
        core::Clip* clip = seq->findClip(clipId_);
        if (clip == nullptr) return;
        clip->speed = oldSpeed_;
        for (const auto& [cid, oldStart] : downstreamShifts_) {
            core::Clip* other = seq->findClip(cid);
            if (other != nullptr) {
                other->seqStart = oldStart;
            }
        }
    }

private:
    core::Id clipId_;
    core::Rational newSpeed_;
    bool ripple_;
    core::Rational oldSpeed_{1, 1};
    std::vector<std::pair<core::Id, core::Rational>> downstreamShifts_;
};

} // namespace

std::unique_ptr<ICommand> makeAddEffectCommand(const core::Id& clipId, core::Effect effect) {
    return std::make_unique<AddEffectCommand>(clipId, std::move(effect));
}

std::unique_ptr<ICommand> makeRemoveEffectCommand(const core::Id& clipId, size_t effectIndex) {
    return std::make_unique<RemoveEffectCommand>(clipId, effectIndex);
}

std::unique_ptr<ICommand> makeUpdateEffectCommand(const core::Id& clipId, size_t effectIndex,
                                                  std::map<std::string, double> params,
                                                  std::map<std::string, std::string> strParams) {
    return std::make_unique<UpdateEffectCommand>(clipId, effectIndex, std::move(params),
                                                 std::move(strParams));
}

std::unique_ptr<ICommand> makeSetClipSpeedCommand(const core::Id& clipId, core::Rational newSpeed,
                                                  bool ripple) {
    return std::make_unique<SetClipSpeedCommand>(clipId, newSpeed, ripple);
}

} // namespace editor::commands
