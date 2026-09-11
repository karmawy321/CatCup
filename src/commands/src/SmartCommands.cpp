#include "commands/SmartCommands.hpp"
#include "core/Ids.hpp"

#include <algorithm>
#include <map>

namespace editor::commands {

namespace {

class SilenceCutCommand final : public ICommand {
public:
    SilenceCutCommand(core::Id trackId, core::Id clipId, std::vector<ai::SilenceInterval> silences)
        : trackId_(std::move(trackId)), clipId_(std::move(clipId)), silences_(std::move(silences)) {}

    std::string label() const override { return "Smart Silence Cut"; }

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
        core::Clip* clip = seq->findClip(clipId_);
        if (clip == nullptr) {
            error = "clip not found: " + clipId_;
            return false;
        }

        // Save original track & clip state for undo
        savedOriginalClip_ = *clip;
        savedTrackClipIds_ = track->clipIds;
        savedDownstreamStarts_.clear();
        generatedClipIds_.clear();

        const core::Rational clipStart = clip->seqStart;
        const core::Rational clipEnd = clip->seqEnd();

        // Filter and sort silences strictly inside clip bounds
        std::vector<std::pair<core::Rational, core::Rational>> validSilences;
        for (const auto& s : silences_) {
            core::Rational sStart = std::max(clipStart, s.startSec);
            core::Rational sEnd = std::min(clipEnd, s.startSec + s.durationSec);
            if (sStart < sEnd) {
                validSilences.push_back({sStart, sEnd});
            }
        }
        std::sort(validSilences.begin(), validSilences.end());

        if (validSilences.empty()) {
            return true; // nothing to cut
        }

        // Build audible segments
        std::vector<std::pair<core::Rational, core::Rational>> audibleSpans;
        core::Rational current = clipStart;
        for (const auto& [sStart, sEnd] : validSilences) {
            if (current < sStart) {
                audibleSpans.push_back({current, sStart});
            }
            current = std::max(current, sEnd);
        }
        if (current < clipEnd) {
            audibleSpans.push_back({current, clipEnd});
        }

        if (audibleSpans.empty()) {
            error = "cannot cut entire clip to nothing";
            return false;
        }

        // Find position of clip in track
        auto it = std::find(track->clipIds.begin(), track->clipIds.end(), clipId_);
        if (it == track->clipIds.end()) {
            error = "clip not in track clip list";
            return false;
        }
        const std::size_t clipIndex = static_cast<std::size_t>(std::distance(track->clipIds.begin(), it));

        // Calculate total silence duration removed
        core::Rational totalAudibleDuration{0};
        for (const auto& [aStart, aEnd] : audibleSpans) {
            totalAudibleDuration = totalAudibleDuration + (aEnd - aStart);
        }
        const core::Rational totalGapRemoved = (clipEnd - clipStart) - totalAudibleDuration;

        // Record downstream clip shifts
        for (std::size_t i = clipIndex + 1; i < track->clipIds.size(); ++i) {
            core::Clip* downstream = seq->findClip(track->clipIds[i]);
            if (downstream != nullptr) {
                savedDownstreamStarts_.push_back({downstream->id, downstream->seqStart});
                downstream->seqStart = downstream->seqStart - totalGapRemoved;
            }
        }

        // Remove original clip from map & track
        seq->clips.erase(clipId_);
        track->clipIds.erase(it);

        // Insert new audible clips rippled together
        core::Rational placeSeqTime = clipStart;
        for (std::size_t i = 0; i < audibleSpans.size(); ++i) {
            const auto& [aStart, aEnd] = audibleSpans[i];
            const core::Rational dur = aEnd - aStart;

            core::Clip subClip = savedOriginalClip_;
            subClip.id = savedOriginalClip_.id + "_aud_" + std::to_string(i + 1);
            subClip.seqStart = placeSeqTime;
            subClip.sourceIn = savedOriginalClip_.mapToSource(aStart);
            subClip.sourceOut = subClip.sourceIn + (dur * savedOriginalClip_.speed);

            seq->clips.emplace(subClip.id, subClip);
            track->clipIds.insert(track->clipIds.begin() + static_cast<ptrdiff_t>(clipIndex + i), subClip.id);
            generatedClipIds_.push_back(subClip.id);

            placeSeqTime = placeSeqTime + dur;
        }

        return true;
    }

    void undo(core::Project& project) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) return;
        core::Track* track = seq->findTrack(trackId_);
        if (track == nullptr) return;

        // Erase generated clips
        for (const auto& gid : generatedClipIds_) {
            seq->clips.erase(gid);
        }

        // Restore original track clip list and original clip
        track->clipIds = savedTrackClipIds_;
        seq->clips.emplace(savedOriginalClip_.id, savedOriginalClip_);

        // Restore downstream starts
        for (const auto& [cid, oldStart] : savedDownstreamStarts_) {
            core::Clip* c = seq->findClip(cid);
            if (c != nullptr) {
                c->seqStart = oldStart;
            }
        }
    }

private:
    core::Id trackId_;
    core::Id clipId_;
    std::vector<ai::SilenceInterval> silences_;

    core::Clip savedOriginalClip_;
    std::vector<core::Id> savedTrackClipIds_;
    std::vector<std::pair<core::Id, core::Rational>> savedDownstreamStarts_;
    std::vector<core::Id> generatedClipIds_;
};

class SceneSplitCommand final : public ICommand {
public:
    SceneSplitCommand(core::Id clipId, std::vector<core::Rational> cutPoints)
        : clipId_(std::move(clipId)), cutPoints_(std::move(cutPoints)) {}

    std::string label() const override { return "Smart Scene Split"; }

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

        core::Track* track = nullptr;
        for (auto& t : seq->tracks) {
            if (std::find(t.clipIds.begin(), t.clipIds.end(), clipId_) != t.clipIds.end()) {
                track = &t;
                break;
            }
        }
        if (track == nullptr) {
            error = "clip track not found";
            return false;
        }

        savedOriginalClip_ = *clip;
        savedTrackClipIds_ = track->clipIds;
        trackId_ = track->id;
        generatedClipIds_.clear();

        // Filter valid cut points strictly inside [seqStart, seqEnd)
        std::vector<core::Rational> validCuts;
        for (const auto& cp : cutPoints_) {
            if (cp > clip->seqStart && cp < clip->seqEnd()) {
                validCuts.push_back(cp);
            }
        }
        std::sort(validCuts.begin(), validCuts.end());
        validCuts.erase(std::unique(validCuts.begin(), validCuts.end()), validCuts.end());

        if (validCuts.empty()) {
            return true; // No cuts inside this clip
        }

        auto it = std::find(track->clipIds.begin(), track->clipIds.end(), clipId_);
        const std::size_t clipIndex = static_cast<std::size_t>(std::distance(track->clipIds.begin(), it));

        // Construct segments [start, end)
        std::vector<std::pair<core::Rational, core::Rational>> segments;
        core::Rational prev = clip->seqStart;
        for (const auto& cp : validCuts) {
            segments.push_back({prev, cp});
            prev = cp;
        }
        segments.push_back({prev, clip->seqEnd()});

        // Remove original clip
        seq->clips.erase(clipId_);
        track->clipIds.erase(it);

        for (std::size_t i = 0; i < segments.size(); ++i) {
            const auto& [segStart, segEnd] = segments[i];
            const core::Rational dur = segEnd - segStart;

            core::Clip subClip = savedOriginalClip_;
            subClip.id = savedOriginalClip_.id + "_scene_" + std::to_string(i + 1);
            subClip.seqStart = segStart;
            subClip.sourceIn = savedOriginalClip_.mapToSource(segStart);
            subClip.sourceOut = subClip.sourceIn + (dur * savedOriginalClip_.speed);

            seq->clips.emplace(subClip.id, subClip);
            track->clipIds.insert(track->clipIds.begin() + static_cast<ptrdiff_t>(clipIndex + i), subClip.id);
            generatedClipIds_.push_back(subClip.id);
        }

        return true;
    }

    void undo(core::Project& project) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) return;
        core::Track* track = seq->findTrack(trackId_);
        if (track == nullptr) return;

        for (const auto& gid : generatedClipIds_) {
            seq->clips.erase(gid);
        }

        track->clipIds = savedTrackClipIds_;
        seq->clips.emplace(savedOriginalClip_.id, savedOriginalClip_);
    }

private:
    core::Id clipId_;
    std::vector<core::Rational> cutPoints_;

    core::Id trackId_;
    core::Clip savedOriginalClip_;
    std::vector<core::Id> savedTrackClipIds_;
    std::vector<core::Id> generatedClipIds_;
};

class AddCaptionsCommand final : public ICommand {
public:
    AddCaptionsCommand(core::Id trackId, std::vector<ai::CaptionCue> cues)
        : trackId_(std::move(trackId)), cues_(std::move(cues)) {}

    std::string label() const override { return "Add Auto Captions"; }

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

        addedClipIds_.clear();
        for (std::size_t i = 0; i < cues_.size(); ++i) {
            const auto& cue = cues_[i];
            core::Clip captionClip;
            captionClip.id = core::IdGenerator::make("caption");
            captionClip.name = "Caption " + std::to_string(i + 1);
            captionClip.text = cue.text;
            captionClip.fontFamily = "Arial";
            captionClip.fontSizePt = 36.0;
            captionClip.sourceIn = core::Rational(0);
            captionClip.sourceOut = cue.durationSec;
            captionClip.seqStart = cue.startSec;

            seq->clips.emplace(captionClip.id, captionClip);
            track->clipIds.push_back(captionClip.id);
            addedClipIds_.push_back(captionClip.id);
        }

        return true;
    }

    void undo(core::Project& project) override {
        core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) return;
        core::Track* track = seq->findTrack(trackId_);
        if (track == nullptr) return;

        for (const auto& cid : addedClipIds_) {
            seq->clips.erase(cid);
            auto it = std::find(track->clipIds.begin(), track->clipIds.end(), cid);
            if (it != track->clipIds.end()) {
                track->clipIds.erase(it);
            }
        }
    }

private:
    core::Id trackId_;
    std::vector<ai::CaptionCue> cues_;
    std::vector<core::Id> addedClipIds_;
};

} // namespace

std::unique_ptr<ICommand> makeSilenceCutCommand(
    const core::Id& trackId,
    const core::Id& clipId,
    const std::vector<ai::SilenceInterval>& silences
) {
    return std::make_unique<SilenceCutCommand>(trackId, clipId, silences);
}

std::unique_ptr<ICommand> makeSceneSplitCommand(
    const core::Id& clipId,
    const std::vector<core::Rational>& cutPoints
) {
    return std::make_unique<SceneSplitCommand>(clipId, cutPoints);
}

std::unique_ptr<ICommand> makeAddCaptionsCommand(
    const core::Id& trackId,
    const std::vector<ai::CaptionCue>& cues
) {
    return std::make_unique<AddCaptionsCommand>(trackId, cues);
}

} // namespace editor::commands
