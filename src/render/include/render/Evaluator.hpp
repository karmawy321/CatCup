#pragma once

// Shared render description: the single source of truth both the realtime
// preview (Stage 1 Qt) and the offline exporter consume. Any effect that
// cannot be expressed here must be marked unavailable — preview and export
// must never disagree silently.

#include "core/Ids.hpp"
#include "core/Model.hpp"
#include "core/Rational.hpp"

#include <vector>

namespace editor::render {

struct PlacedClip {
    core::Id clipId;
    core::Id assetId;
    core::TrackKind trackKind = core::TrackKind::Video;
    core::Rational sourceTime{0}; // evaluated source position for this frame
    int zOrder = 0;               // track order, then clip order within track
    bool isText = false;
    core::Transform transform{};
    double opacity = 1.0;

    // Transition blend slot (Stage 2)
    bool inTransition = false;
    core::Id transitionId;
    std::string transitionType;
    core::Id secondaryClipId;
    core::Id secondaryAssetId;
    core::Rational secondarySourceTime{0};
    double blendFactor = 0.0; // 0.0 = primary/outgoing, 1.0 = secondary/incoming

    // Effects stack (Stage 3)
    std::vector<core::Effect> effects;
    std::vector<core::Effect> secondaryEffects;
};

struct FramePlan {
    core::Rational time{0};
    std::vector<PlacedClip> layers; // back-to-front
};

class Evaluator {
public:
    /// Clips active at time t (video+text layers; audio reported separately).
    static FramePlan evaluateVideoAt(const core::Sequence& seq, const core::Rational& t);
    /// Audio clips active at time t (for the mixer clock).
    static std::vector<PlacedClip> evaluateAudioAt(const core::Sequence& seq,
                                                   const core::Rational& t);
    static core::Rational sequenceDuration(const core::Sequence& seq);
};

} // namespace editor::render
