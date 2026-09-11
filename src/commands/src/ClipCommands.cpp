#include "commands/ClipCommands.hpp"

#include <algorithm>

namespace editor::commands {
namespace {

const core::Asset* findAsset(const core::Project& project, const core::Id& id) {
    if (id.empty()) {
        return nullptr;
    }
    const auto it = project.assets.find(id);
    return it == project.assets.end() ? nullptr : &it->second;
}

bool detachFromTrack(core::Sequence& seq, const core::Id& trackId, const core::Id& clipId) {
    core::Track* track = seq.findTrack(trackId);
    if (track == nullptr) {
        return false;
    }
    auto& ids = track->clipIds;
    const auto it = std::find(ids.begin(), ids.end(), clipId);
    if (it == ids.end()) {
        return false;
    }
    ids.erase(it);
    return true;
}

class AddClipCommand final : public ICommand {
public:
    AddClipCommand(core::Id trackId, core::Clip clip)
        : trackId_(std::move(trackId)), clip_(std::move(clip)) {}

    std::string label() const override { return "Add clip " + clip_.name; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        core::Track* track = seq->findTrack(trackId_);
        if (track == nullptr) {
            error = "track not found: " + trackId_;
            return false;
        }
        if (track->locked) {
            error = "track is locked: " + track->name;
            return false;
        }
        const core::Asset* asset = findAsset(project, clip_.assetId);
        if (const auto r = clip_.validate(asset); r.isErr()) {
            // Allow text clips (empty assetId) and forward-reference clips whose
            // asset check is deferred to project validation; only reject shape
            // errors here, not missing-asset references.
            if (!(!clip_.assetId.empty() && asset == nullptr)) {
                error = r.error();
                return false;
            }
        }
        if (seq->clips.count(clip_.id) != 0) {
            error = "clip id already exists: " + clip_.id;
            return false;
        }
        seq->clips.emplace(clip_.id, clip_);
        track->clipIds.push_back(clip_.id);
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            detachFromTrack(*seq, trackId_, clip_.id);
            seq->clips.erase(clip_.id);
        }
    }

private:
    core::Id trackId_;
    core::Clip clip_;
};

class RemoveClipCommand final : public ICommand {
public:
    RemoveClipCommand(core::Id trackId, core::Id clipId)
        : trackId_(std::move(trackId)), clipId_(std::move(clipId)) {}

    std::string label() const override { return "Remove clip"; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        core::Track* track = seq->findTrack(trackId_);
        if (track == nullptr) {
            error = "track not found: " + trackId_;
            return false;
        }
        if (track->locked) {
            error = "track is locked: " + track->name;
            return false;
        }
        const auto it = seq->clips.find(clipId_);
        if (it == seq->clips.end()) {
            error = "clip not found: " + clipId_;
            return false;
        }
        snapshot_ = it->second;
        const auto pos =
            std::find(track->clipIds.begin(), track->clipIds.end(), clipId_);
        index_ = pos == track->clipIds.end()
                     ? 0
                     : static_cast<std::size_t>(pos - track->clipIds.begin());
        removedTransitions_.clear();
        for (auto tit = seq->transitions.begin(); tit != seq->transitions.end(); ) {
            if (tit->fromClipId == clipId_ || tit->toClipId == clipId_) {
                removedTransitions_.push_back(*tit);
                tit = seq->transitions.erase(tit);
            } else {
                ++tit;
            }
        }
        detachFromTrack(*seq, trackId_, clipId_);
        seq->clips.erase(it);
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            seq->clips.emplace(snapshot_.id, snapshot_);
            if (core::Track* track = seq->findTrack(trackId_)) {
                const std::size_t at = (std::min)(index_, track->clipIds.size());
                track->clipIds.insert(track->clipIds.begin() + at, snapshot_.id);
            }
            for (const auto& tr : removedTransitions_) {
                seq->transitions.push_back(tr);
            }
        }
    }

private:
    core::Id trackId_;
    core::Id clipId_;
    core::Clip snapshot_;
    std::size_t index_ = 0;
    std::vector<core::Transition> removedTransitions_;
};

class MoveClipCommand final : public ICommand {
public:
    MoveClipCommand(core::Id clipId, core::Rational newStart)
        : clipId_(std::move(clipId)), newStart_(newStart) {}

    std::string label() const override { return "Move clip"; }

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
        if (newStart_.isNegative()) {
            error = "clip start cannot be negative";
            return false;
        }
        if (!haveOld_) {
            oldStart_ = clip->seqStart;
            haveOld_ = true;
        }
        clip->seqStart = newStart_;
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->seqStart = oldStart_;
            }
        }
    }

private:
    core::Id clipId_;
    core::Rational newStart_;
    core::Rational oldStart_{0};
    bool haveOld_ = false;
};

class TrimClipCommand final : public ICommand {
public:
    TrimClipCommand(core::Id clipId, core::Rational in, core::Rational out,
                    core::Rational start)
        : clipId_(std::move(clipId)), newIn_(in), newOut_(out), newStart_(start) {}

    std::string label() const override { return "Trim clip"; }

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
        if (!(newIn_ < newOut_)) {
            error = "trim requires sourceIn < sourceOut";
            return false;
        }
        if (newIn_.isNegative() || newStart_.isNegative()) {
            error = "trim bounds cannot be negative";
            return false;
        }
        const core::Asset* asset = findAsset(project, clip->assetId);
        core::Clip probe = *clip;
        probe.sourceIn = newIn_;
        probe.sourceOut = newOut_;
        probe.seqStart = newStart_;
        if (const auto r = probe.validate(asset); r.isErr()) {
            error = r.error();
            return false;
        }
        if (!haveOld_) {
            oldIn_ = clip->sourceIn;
            oldOut_ = clip->sourceOut;
            oldStart_ = clip->seqStart;
            haveOld_ = true;
        }
        clip->sourceIn = newIn_;
        clip->sourceOut = newOut_;
        clip->seqStart = newStart_;
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->sourceIn = oldIn_;
                clip->sourceOut = oldOut_;
                clip->seqStart = oldStart_;
            }
        }
    }

private:
    core::Id clipId_;
    core::Rational newIn_, newOut_, newStart_;
    core::Rational oldIn_{0}, oldOut_{0}, oldStart_{0};
    bool haveOld_ = false;
};

class SplitClipCommand final : public ICommand {
public:
    SplitClipCommand(core::Id clipId, core::Rational at, core::Id rightId)
        : clipId_(std::move(clipId)), at_(at), rightId_(std::move(rightId)) {}

    std::string label() const override { return "Split clip"; }

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
        if (!(clip->seqStart < at_ && at_ < clip->seqEnd())) {
            error = "split point is outside the clip";
            return false;
        }
        if (seq->clips.count(rightId_) != 0) {
            error = "clip id already exists: " + rightId_;
            return false;
        }
        // Owning track lookup.
        core::Track* owner = nullptr;
        std::size_t ownerIndex = 0;
        for (auto& track : seq->tracks) {
            const auto it = std::find(track.clipIds.begin(), track.clipIds.end(), clipId_);
            if (it != track.clipIds.end()) {
                owner = &track;
                ownerIndex = static_cast<std::size_t>(it - track.clipIds.begin());
                break;
            }
        }
        if (owner == nullptr) {
            error = "clip is not placed on any track";
            return false;
        }
        if (owner->locked) {
            error = "track is locked: " + owner->name;
            return false;
        }
        if (!haveOld_) {
            oldOut_ = clip->sourceOut;
            haveOld_ = true;
        }
        const core::Rational cutSource = clip->mapToSource(at_);
        core::Clip right = *clip;
        right.id = rightId_;
        right.sourceIn = cutSource;
        right.seqStart = at_;

        clip->sourceOut = cutSource;

        seq->clips.emplace(right.id, right);
        owner->clipIds.insert(owner->clipIds.begin() + ownerIndex + 1, right.id);
        ownerTrackId_ = owner->id;
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            detachFromTrack(*seq, ownerTrackId_, rightId_);
            seq->clips.erase(rightId_);
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->sourceOut = oldOut_;
            }
        }
    }

private:
    core::Id clipId_;
    core::Rational at_;
    core::Id rightId_;
    core::Id ownerTrackId_;
    core::Rational oldOut_{0};
    bool haveOld_ = false;
};

} // namespace

std::unique_ptr<ICommand> makeAddClipCommand(const core::Id& trackId, core::Clip clip) {
    return std::make_unique<AddClipCommand>(trackId, std::move(clip));
}

std::unique_ptr<ICommand> makeRemoveClipCommand(const core::Id& trackId,
                                                const core::Id& clipId) {
    return std::make_unique<RemoveClipCommand>(trackId, clipId);
}

std::unique_ptr<ICommand> makeMoveClipCommand(const core::Id& clipId,
                                              core::Rational newStart) {
    return std::make_unique<MoveClipCommand>(clipId, newStart);
}

std::unique_ptr<ICommand> makeTrimClipCommand(const core::Id& clipId, core::Rational newIn,
                                              core::Rational newOut, core::Rational newStart) {
    return std::make_unique<TrimClipCommand>(clipId, newIn, newOut, newStart);
}

std::unique_ptr<ICommand> makeSplitClipCommand(const core::Id& clipId, core::Rational at,
                                               core::Id rightId) {
    return std::make_unique<SplitClipCommand>(clipId, at, std::move(rightId));
}

class SetTransformCommand final : public ICommand {
public:
    SetTransformCommand(core::Id clipId, core::Transform transform)
        : clipId_(std::move(clipId)), next_(transform) {}

    std::string label() const override { return "Set transform"; }

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
        if (!(next_.scale > 0)) {
            error = "scale must be positive";
            return false;
        }
        if (!haveOld_) {
            prev_ = clip->transform;
            haveOld_ = true;
        }
        clip->transform = next_;
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->transform = prev_;
            }
        }
    }

private:
    core::Id clipId_;
    core::Transform next_;
    core::Transform prev_;
    bool haveOld_ = false;
};

class SetTextCommand final : public ICommand {
public:
    SetTextCommand(core::Id clipId, std::string text, std::string fontFamily,
                   double fontSizePt)
        : clipId_(std::move(clipId)), text_(std::move(text)),
          fontFamily_(std::move(fontFamily)), fontSizePt_(fontSizePt) {}

    std::string label() const override { return "Set title text"; }

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
        if (!(fontSizePt_ > 0)) {
            error = "font size must be positive";
            return false;
        }
        if (!haveOld_) {
            prevText_ = clip->text;
            prevFont_ = clip->fontFamily;
            prevSize_ = clip->fontSizePt;
            haveOld_ = true;
        }
        clip->text = text_;
        clip->fontFamily = fontFamily_;
        clip->fontSizePt = fontSizePt_;
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->text = prevText_;
                clip->fontFamily = prevFont_;
                clip->fontSizePt = prevSize_;
            }
        }
    }

private:
    core::Id clipId_;
    std::string text_, fontFamily_;
    double fontSizePt_;
    std::string prevText_, prevFont_;
    double prevSize_ = 48.0;
    bool haveOld_ = false;
};

class RippleDeleteClipCommand final : public ICommand {
public:
    RippleDeleteClipCommand(core::Id trackId, core::Id clipId)
        : trackId_(std::move(trackId)), clipId_(std::move(clipId)) {}

    std::string label() const override { return "Ripple delete clip"; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        core::Track* track = seq->findTrack(trackId_);
        if (track == nullptr) {
            error = "track not found: " + trackId_;
            return false;
        }
        if (track->locked) {
            error = "track is locked: " + track->name;
            return false;
        }
        const auto it = seq->clips.find(clipId_);
        if (it == seq->clips.end()) {
            error = "clip not found: " + clipId_;
            return false;
        }
        snapshot_ = it->second;
        const auto pos = std::find(track->clipIds.begin(), track->clipIds.end(), clipId_);
        index_ = pos == track->clipIds.end() ? 0 : static_cast<std::size_t>(pos - track->clipIds.begin());
        const core::Rational dur = snapshot_.seqDuration();
        const core::Rational cutEnd = snapshot_.seqEnd();

        shiftedClips_.clear();
        for (const auto& cid : track->clipIds) {
            if (cid == clipId_) continue;
            auto cit = seq->clips.find(cid);
            if (cit != seq->clips.end() && cit->second.seqStart >= cutEnd) {
                shiftedClips_.push_back({cid, cit->second.seqStart});
                cit->second.seqStart = cit->second.seqStart - dur;
            }
        }

        removedTransitions_.clear();
        for (auto tit = seq->transitions.begin(); tit != seq->transitions.end(); ) {
            if (tit->fromClipId == clipId_ || tit->toClipId == clipId_) {
                removedTransitions_.push_back(*tit);
                tit = seq->transitions.erase(tit);
            } else {
                ++tit;
            }
        }

        detachFromTrack(*seq, trackId_, clipId_);
        seq->clips.erase(it);
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            seq->clips.emplace(snapshot_.id, snapshot_);
            if (core::Track* track = seq->findTrack(trackId_)) {
                const std::size_t at = (std::min)(index_, track->clipIds.size());
                track->clipIds.insert(track->clipIds.begin() + at, snapshot_.id);
            }
            for (const auto& [cid, oldStart] : shiftedClips_) {
                if (auto cit = seq->clips.find(cid); cit != seq->clips.end()) {
                    cit->second.seqStart = oldStart;
                }
            }
            for (const auto& tr : removedTransitions_) {
                seq->transitions.push_back(tr);
            }
        }
    }

private:
    core::Id trackId_;
    core::Id clipId_;
    core::Clip snapshot_;
    std::size_t index_ = 0;
    std::vector<std::pair<core::Id, core::Rational>> shiftedClips_;
    std::vector<core::Transition> removedTransitions_;
};

class RippleTrimClipCommand final : public ICommand {
public:
    RippleTrimClipCommand(core::Id clipId, core::Rational newIn, core::Rational newOut,
                          core::Rational newStart)
        : clipId_(std::move(clipId)), newIn_(newIn), newOut_(newOut), newStart_(newStart) {}

    std::string label() const override { return "Ripple trim clip"; }

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
        if (!(newIn_ < newOut_)) {
            error = "newIn must be < newOut";
            return false;
        }
        if (newIn_.isNegative() || newStart_.isNegative()) {
            error = "times cannot be negative";
            return false;
        }
        const auto* asset = findAsset(project, clip->assetId);
        if (asset != nullptr && asset->duration.num() > 0 && newOut_ > asset->duration) {
            error = "trimmed range extends past asset duration";
            return false;
        }

        prevIn_ = clip->sourceIn;
        prevOut_ = clip->sourceOut;
        prevStart_ = clip->seqStart;
        const core::Rational oldEnd = clip->seqEnd();
        const core::Rational newDur = newOut_ - newIn_;
        const core::Rational delta = (newStart_ + newDur) - oldEnd;

        const core::Track* trk = nullptr;
        for (const auto& t : seq->tracks) {
            for (const auto& cid : t.clipIds) {
                if (cid == clipId_) {
                    trk = &t;
                    break;
                }
            }
            if (trk != nullptr) break;
        }

        shiftedClips_.clear();
        if (trk != nullptr && !delta.isZero()) {
            for (const auto& cid : trk->clipIds) {
                if (cid == clipId_) continue;
                auto cit = seq->clips.find(cid);
                if (cit != seq->clips.end() && cit->second.seqStart >= oldEnd) {
                    shiftedClips_.push_back({cid, cit->second.seqStart});
                    cit->second.seqStart = cit->second.seqStart + delta;
                }
            }
        }

        clip->sourceIn = newIn_;
        clip->sourceOut = newOut_;
        clip->seqStart = newStart_;
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->sourceIn = prevIn_;
                clip->sourceOut = prevOut_;
                clip->seqStart = prevStart_;
            }
            for (const auto& [cid, oldStart] : shiftedClips_) {
                if (auto cit = seq->clips.find(cid); cit != seq->clips.end()) {
                    cit->second.seqStart = oldStart;
                }
            }
        }
    }

private:
    core::Id clipId_;
    core::Rational newIn_, newOut_, newStart_;
    core::Rational prevIn_{0}, prevOut_{0}, prevStart_{0};
    std::vector<std::pair<core::Id, core::Rational>> shiftedClips_;
};

class SetOpacityCommand final : public ICommand {
public:
    SetOpacityCommand(core::Id clipId, double opacity)
        : clipId_(std::move(clipId)), opacity_(opacity) {}

    std::string label() const override { return "Set clip opacity"; }

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
        if (opacity_ < 0.0 || opacity_ > 1.0) {
            error = "opacity must be in [0, 1]";
            return false;
        }
        prevOpacity_ = clip->opacity;
        clip->opacity = opacity_;
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->opacity = prevOpacity_;
            }
        }
    }

private:
    core::Id clipId_;
    double opacity_ = 1.0;
    double prevOpacity_ = 1.0;
};

std::unique_ptr<ICommand> makeSetTransformCommand(const core::Id& clipId,
                                                  core::Transform transform) {
    return std::make_unique<SetTransformCommand>(clipId, transform);
}

std::unique_ptr<ICommand> makeSetTextCommand(const core::Id& clipId, std::string text,
                                             std::string fontFamily, double fontSizePt) {
    return std::make_unique<SetTextCommand>(clipId, std::move(text), std::move(fontFamily),
                                            fontSizePt);
}

std::unique_ptr<ICommand> makeRippleDeleteClipCommand(const core::Id& trackId, const core::Id& clipId) {
    return std::make_unique<RippleDeleteClipCommand>(trackId, clipId);
}

std::unique_ptr<ICommand> makeRippleTrimClipCommand(const core::Id& clipId, core::Rational newIn,
                                                    core::Rational newOut, core::Rational newStart) {
    return std::make_unique<RippleTrimClipCommand>(clipId, newIn, newOut, newStart);
}

std::unique_ptr<ICommand> makeSetOpacityCommand(const core::Id& clipId, double opacity) {
    return std::make_unique<SetOpacityCommand>(clipId, opacity);
}

class SetKeyframeCommand final : public ICommand {
public:
    SetKeyframeCommand(core::Id clipId, core::Keyframe kf)
        : clipId_(std::move(clipId)), kf_(std::move(kf)) {}

    std::string label() const override { return "Set keyframe"; }

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
        prevKeyframes_ = clip->keyframes;

        // Insert or update keyframe at kf_.seqTime
        bool updated = false;
        for (auto& k : clip->keyframes) {
            if (k.seqTime == kf_.seqTime) {
                k = kf_;
                updated = true;
                break;
            }
        }
        if (!updated) {
            clip->keyframes.push_back(kf_);
            std::sort(clip->keyframes.begin(), clip->keyframes.end(),
                      [](const auto& a, const auto& b) { return a.seqTime < b.seqTime; });
        }
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->keyframes = prevKeyframes_;
            }
        }
    }

private:
    core::Id clipId_;
    core::Keyframe kf_;
    std::vector<core::Keyframe> prevKeyframes_;
};

class RemoveKeyframeCommand final : public ICommand {
public:
    RemoveKeyframeCommand(core::Id clipId, core::Rational seqTime)
        : clipId_(std::move(clipId)), seqTime_(seqTime) {}

    std::string label() const override { return "Remove keyframe"; }

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
        prevKeyframes_ = clip->keyframes;

        for (auto it = clip->keyframes.begin(); it != clip->keyframes.end(); ++it) {
            if (it->seqTime == seqTime_) {
                clip->keyframes.erase(it);
                return true;
            }
        }
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->keyframes = prevKeyframes_;
            }
        }
    }

private:
    core::Id clipId_;
    core::Rational seqTime_;
    std::vector<core::Keyframe> prevKeyframes_;
};

class SetFadeCommand final : public ICommand {
public:
    SetFadeCommand(core::Id clipId, double fadeInSec, double fadeOutSec)
        : clipId_(std::move(clipId)), fadeIn_(fadeInSec), fadeOut_(fadeOutSec) {}

    std::string label() const override { return "Set clip fade"; }

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
        prevFadeIn_ = clip->fadeInSec;
        prevFadeOut_ = clip->fadeOutSec;
        clip->fadeInSec = std::max(0.0, fadeIn_);
        clip->fadeOutSec = std::max(0.0, fadeOut_);
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            if (core::Clip* clip = seq->findClip(clipId_)) {
                clip->fadeInSec = prevFadeIn_;
                clip->fadeOutSec = prevFadeOut_;
            }
        }
    }

private:
    core::Id clipId_;
    double fadeIn_ = 0.0;
    double fadeOut_ = 0.0;
    double prevFadeIn_ = 0.0;
    double prevFadeOut_ = 0.0;
};

class SetSequenceFormatCommand final : public ICommand {
public:
    SetSequenceFormatCommand(std::int64_t width, std::int64_t height)
        : width_(width), height_(height) {}

    std::string label() const override { return "Change canvas resolution"; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            error = "no active sequence";
            return false;
        }
        if (width_ <= 0 || height_ <= 0) {
            error = "canvas dimensions must be positive";
            return false;
        }
        prevWidth_ = seq->width;
        prevHeight_ = seq->height;
        seq->width = width_;
        seq->height = height_;
        return true;
    }

    void undo(core::Project& project) override {
        if (core::Sequence* seq = project.activeSequence()) {
            seq->width = prevWidth_;
            seq->height = prevHeight_;
        }
    }

private:
    std::int64_t width_ = 1280;
    std::int64_t height_ = 720;
    std::int64_t prevWidth_ = 1280;
    std::int64_t prevHeight_ = 720;
};

std::unique_ptr<ICommand> makeSetKeyframeCommand(const core::Id& clipId, core::Keyframe kf) {
    return std::make_unique<SetKeyframeCommand>(clipId, std::move(kf));
}

std::unique_ptr<ICommand> makeRemoveKeyframeCommand(const core::Id& clipId, core::Rational seqTime) {
    return std::make_unique<RemoveKeyframeCommand>(clipId, seqTime);
}

std::unique_ptr<ICommand> makeSetFadeCommand(const core::Id& clipId, double fadeInSec, double fadeOutSec) {
    return std::make_unique<SetFadeCommand>(clipId, fadeInSec, fadeOutSec);
}

std::unique_ptr<ICommand> makeSetSequenceFormatCommand(std::int64_t width, std::int64_t height) {
    return std::make_unique<SetSequenceFormatCommand>(width, height);
}

class MoveClipToTrackCommand final : public ICommand {
public:
    MoveClipToTrackCommand(core::Id clipId, core::Id newTrackId, core::Rational newStart)
        : clipId_(std::move(clipId)), newTrackId_(std::move(newTrackId)), newStart_(newStart) {}

    std::string label() const override { return "Move clip to track"; }

    bool execute(core::Project& project, std::string& error) override {
        core::Sequence* seq = project.activeSequence();
        if (!seq) {
            error = "no active sequence";
            return false;
        }
        core::Clip* clip = seq->findClip(clipId_);
        if (!clip) {
            error = "clip not found: " + clipId_;
            return false;
        }
        core::Track* destTrack = seq->findTrack(newTrackId_);
        if (!destTrack) {
            error = "destination track not found: " + newTrackId_;
            return false;
        }
        if (newStart_.isNegative()) {
            error = "clip start cannot be negative";
            return false;
        }

        // Find old track
        core::Track* srcTrack = nullptr;
        for (auto& t : seq->tracks) {
            auto it = std::find(t.clipIds.begin(), t.clipIds.end(), clipId_);
            if (it != t.clipIds.end()) {
                srcTrack = &t;
                oldIndex_ = static_cast<std::size_t>(std::distance(t.clipIds.begin(), it));
                break;
            }
        }
        if (!srcTrack) {
            error = "clip not found on any track";
            return false;
        }

        if (!haveOld_) {
            oldTrackId_ = srcTrack->id;
            oldStart_ = clip->seqStart;
            haveOld_ = true;
        }

        if (srcTrack->id != newTrackId_) {
            srcTrack->clipIds.erase(srcTrack->clipIds.begin() + oldIndex_);
            destTrack->clipIds.push_back(clipId_);
        }
        clip->seqStart = newStart_;
        return true;
    }

    void undo(core::Project& project) override {
        core::Sequence* seq = project.activeSequence();
        if (!seq) return;
        core::Clip* clip = seq->findClip(clipId_);
        if (!clip) return;

        if (oldTrackId_ != newTrackId_) {
            core::Track* destTrack = seq->findTrack(newTrackId_);
            core::Track* srcTrack = seq->findTrack(oldTrackId_);
            if (destTrack) {
                auto it = std::find(destTrack->clipIds.begin(), destTrack->clipIds.end(), clipId_);
                if (it != destTrack->clipIds.end()) {
                    destTrack->clipIds.erase(it);
                }
            }
            if (srcTrack) {
                const auto at = (std::min)(oldIndex_, srcTrack->clipIds.size());
                srcTrack->clipIds.insert(srcTrack->clipIds.begin() + at, clipId_);
            }
        }
        clip->seqStart = oldStart_;
    }

private:
    core::Id clipId_;
    core::Id newTrackId_;
    core::Rational newStart_;
    core::Id oldTrackId_;
    core::Rational oldStart_{0};
    std::size_t oldIndex_ = 0;
    bool haveOld_ = false;
};

std::unique_ptr<ICommand> makeMoveClipToTrackCommand(const core::Id& clipId, const core::Id& newTrackId, core::Rational newStart) {
    return std::make_unique<MoveClipToTrackCommand>(clipId, newTrackId, newStart);
}

} // namespace editor::commands

