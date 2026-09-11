#include "render/Evaluator.hpp"

#include <algorithm>
#include <set>

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

        std::set<core::Id> handledClips;
        for (const auto& tr : seq.transitions) {
            if (tr.trackId != track.id) {
                continue;
            }
            const core::Clip* from = seq.findClip(tr.fromClipId);
            const core::Clip* to = seq.findClip(tr.toClipId);
            if (from == nullptr || to == nullptr || !from->enabled || !to->enabled) {
                continue;
            }
            const core::TimeRange range = tr.timeRange(*from, *to);
            if (range.contains(t) && range.duration.num() > 0) {
                handledClips.insert(from->id);
                handledClips.insert(to->id);

                PlacedClip placed;
                placed.clipId = from->id;
                placed.assetId = from->assetId;
                placed.trackKind = track.kind;
                placed.sourceTime = from->mapToSource(t);
                placed.zOrder = z++;
                placed.isText = false;
                placed.transform = from->evaluateTransformAt(t);
                placed.opacity = from->evaluateOpacityAt(t);
                placed.effects = from->effects;
                placed.secondaryEffects = to->effects;

                placed.inTransition = true;
                placed.transitionId = tr.id;
                placed.transitionType = tr.type;
                placed.secondaryClipId = to->id;
                placed.secondaryAssetId = to->assetId;
                placed.secondarySourceTime = to->mapToSource(t);

                const core::Rational offset = t - range.start;
                double progress = static_cast<double>(offset) / static_cast<double>(range.duration);
                if (progress < 0.0) progress = 0.0;
                if (progress > 1.0) progress = 1.0;
                if (tr.easing == "ease_in_out") {
                    progress = progress * progress * (3.0 - 2.0 * progress);
                }
                placed.blendFactor = progress;

                plan.layers.push_back(std::move(placed));
            }
        }

        for (const auto& clipId : track.clipIds) {
            if (handledClips.count(clipId) > 0) {
                continue;
            }
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
                placed.transform = clip.evaluateTransformAt(t);
                placed.opacity = clip.evaluateOpacityAt(t);
                placed.effects = clip.effects;
                plan.layers.push_back(std::move(placed));
            }
        }
    }
    return plan;
}

std::vector<PlacedClip> Evaluator::evaluateAudioAt(const core::Sequence& seq,
                                                   const core::Rational& t) {
    std::vector<PlacedClip> out;
    int z = 0;
    for (const auto& track : seq.tracks) {
        if (track.kind != core::TrackKind::Audio) {
            continue;
        }
        if (track.muted) {
            continue;
        }

        std::set<core::Id> handledClips;
        for (const auto& tr : seq.transitions) {
            if (tr.trackId != track.id) {
                continue;
            }
            const core::Clip* from = seq.findClip(tr.fromClipId);
            const core::Clip* to = seq.findClip(tr.toClipId);
            if (from == nullptr || to == nullptr || !from->enabled || !to->enabled) {
                continue;
            }
            const core::TimeRange range = tr.timeRange(*from, *to);
            if (range.contains(t) && range.duration.num() > 0) {
                handledClips.insert(from->id);
                handledClips.insert(to->id);

                const core::Rational offset = t - range.start;
                double progress = static_cast<double>(offset) / static_cast<double>(range.duration);
                if (progress < 0.0) progress = 0.0;
                if (progress > 1.0) progress = 1.0;
                if (tr.easing == "ease_in_out") {
                    progress = progress * progress * (3.0 - 2.0 * progress);
                }

                // Outgoing clip
                PlacedClip placedFrom;
                placedFrom.clipId = from->id;
                placedFrom.assetId = from->assetId;
                placedFrom.trackKind = track.kind;
                placedFrom.sourceTime = from->mapToSource(t);
                placedFrom.zOrder = z++;
                placedFrom.opacity = from->evaluateOpacityAt(t) * (1.0 - progress);
                out.push_back(std::move(placedFrom));

                // Incoming clip
                PlacedClip placedTo;
                placedTo.clipId = to->id;
                placedTo.assetId = to->assetId;
                placedTo.trackKind = track.kind;
                placedTo.sourceTime = to->mapToSource(t);
                placedTo.zOrder = z++;
                placedTo.opacity = to->evaluateOpacityAt(t) * progress;
                out.push_back(std::move(placedTo));
            }
        }

        for (const auto& clipId : track.clipIds) {
            if (handledClips.count(clipId) > 0) {
                continue;
            }
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
                placed.opacity = clip.evaluateOpacityAt(t);
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
    for (const auto& tr : seq.transitions) {
        const auto* from = seq.findClip(tr.fromClipId);
        const auto* to = seq.findClip(tr.toClipId);
        if (from != nullptr && to != nullptr) {
            const auto r = tr.timeRange(*from, *to);
            if (end < r.end()) {
                end = r.end();
            }
        }
    }
    return end;
}

} // namespace editor::render
