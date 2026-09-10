#include "render/Evaluator.hpp"

namespace editor::render {

FramePlan Evaluator::evaluateVideoAt(const core::Sequence& seq, const core::Rational& t) {
    FramePlan plan;
    plan.time = t;
    int z = 0;
    for (const auto& track : seq.tracks) {
        if (track.kind != core::TrackKind::Video && track.kind != core::TrackKind::Text) {
            continue;
        }
        if (!track.visible) {
            continue;
        }
        for (const auto& clipId : track.clipIds) {
            const auto it = seq.clips.find(clipId);
            if (it == seq.clips.end()) {
                continue;
            }
            const core::Clip& clip = it->second;
            if (!clip.enabled) {
                continue;
            }
            if (clip.seqRange().contains(t)) {
                PlacedClip placed;
                placed.clipId = clip.id;
                placed.assetId = clip.assetId;
                placed.trackKind = track.kind;
                placed.sourceTime = clip.mapToSource(t);
                placed.zOrder = z++;
                placed.isText = track.kind == core::TrackKind::Text || !clip.text.empty();
                plan.layers.push_back(std::move(placed));
            }
        }
    }
    return plan;
}

std::vector<PlacedClip> Evaluator::evaluateAudioAt(const core::Sequence& seq,
                                                   const core::Rational& t) {
    std::vector<PlacedClip> out;
    for (const auto& track : seq.tracks) {
        if (track.kind != core::TrackKind::Audio) {
            continue;
        }
        if (track.muted) {
            continue;
        }
        for (const auto& clipId : track.clipIds) {
            const auto it = seq.clips.find(clipId);
            if (it == seq.clips.end()) {
                continue;
            }
            const core::Clip& clip = it->second;
            if (!clip.enabled) {
                continue;
            }
            if (clip.seqRange().contains(t)) {
                PlacedClip placed;
                placed.clipId = clip.id;
                placed.assetId = clip.assetId;
                placed.trackKind = track.kind;
                placed.sourceTime = clip.mapToSource(t);
                placed.zOrder = 0;
                out.push_back(std::move(placed));
            }
        }
    }
    return out;
}

core::Rational Evaluator::sequenceDuration(const core::Sequence& seq) {
    core::Rational end(0);
    for (const auto& [id, clip] : seq.clips) {
        if (!clip.enabled) {
            continue;
        }
        if (end < clip.seqEnd()) {
            end = clip.seqEnd();
        }
    }
    return end;
}

} // namespace editor::render
