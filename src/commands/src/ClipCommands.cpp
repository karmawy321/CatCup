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
        }
    }

private:
    core::Id trackId_;
    core::Id clipId_;
    core::Clip snapshot_;
    std::size_t index_ = 0;
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

std::unique_ptr<ICommand> makeSetTransformCommand(const core::Id& clipId,
                                                  core::Transform transform) {
    return std::make_unique<SetTransformCommand>(clipId, transform);
}

std::unique_ptr<ICommand> makeSetTextCommand(const core::Id& clipId, std::string text,
                                             std::string fontFamily, double fontSizePt) {
    return std::make_unique<SetTextCommand>(clipId, std::move(text), std::move(fontFamily),
                                            fontSizePt);
}

} // namespace editor::commands
