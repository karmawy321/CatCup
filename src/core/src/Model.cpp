#include "core/Model.hpp"

namespace editor::core {

const char* toString(AssetKind k) noexcept {
    switch (k) {
    case AssetKind::Video: return "video";
    case AssetKind::Audio: return "audio";
    case AssetKind::Image: return "image";
    case AssetKind::Text: return "text";
    }
    return "video";
}

const char* toString(TrackKind k) noexcept {
    switch (k) {
    case TrackKind::Video: return "video";
    case TrackKind::Audio: return "audio";
    case TrackKind::Text: return "text";
    }
    return "video";
}

const char* toString(TransitionAlignment a) noexcept {
    switch (a) {
    case TransitionAlignment::CenterOnCut: return "center";
    case TransitionAlignment::StartOnCut: return "start";
    case TransitionAlignment::EndOnCut: return "end";
    }
    return "center";
}

Result<AssetKind> assetKindFromString(const std::string& s) {
    if (s == "video") {
        return Result<AssetKind>::ok(AssetKind::Video);
    }
    if (s == "audio") {
        return Result<AssetKind>::ok(AssetKind::Audio);
    }
    if (s == "image") {
        return Result<AssetKind>::ok(AssetKind::Image);
    }
    if (s == "text") {
        return Result<AssetKind>::ok(AssetKind::Text);
    }
    return Result<AssetKind>::fail("unknown asset kind: " + s);
}

Result<TrackKind> trackKindFromString(const std::string& s) {
    if (s == "video") {
        return Result<TrackKind>::ok(TrackKind::Video);
    }
    if (s == "audio") {
        return Result<TrackKind>::ok(TrackKind::Audio);
    }
    if (s == "text") {
        return Result<TrackKind>::ok(TrackKind::Text);
    }
    return Result<TrackKind>::fail("unknown track kind: " + s);
}

Result<TransitionAlignment> transitionAlignmentFromString(const std::string& s) {
    if (s == "center") {
        return Result<TransitionAlignment>::ok(TransitionAlignment::CenterOnCut);
    }
    if (s == "start") {
        return Result<TransitionAlignment>::ok(TransitionAlignment::StartOnCut);
    }
    if (s == "end") {
        return Result<TransitionAlignment>::ok(TransitionAlignment::EndOnCut);
    }
    return Result<TransitionAlignment>::fail("unknown transition alignment: " + s);
}

Result<void> Clip::validate(const Asset* assetOrNull) const {
    if (id.empty()) {
        return Result<void>::fail("clip id is empty");
    }
    if (opacity < 0.0 || opacity > 1.0) {
        return Result<void>::fail("clip opacity must be in [0, 1] (clip " + id + ")");
    }
    if (speed.num() <= 0) {
        return Result<void>::fail("clip speed must be positive (clip " + id + ")");
    }
    if (!(sourceIn < sourceOut)) {
        return Result<void>::fail("clip sourceIn must be < sourceOut (clip " + id + ")");
    }
    if (sourceIn.isNegative()) {
        return Result<void>::fail("clip sourceIn is negative (clip " + id + ")");
    }
    if (seqStart.isNegative()) {
        return Result<void>::fail("clip seqStart is negative (clip " + id + ")");
    }
    if (assetOrNull != nullptr && !assetId.empty()) {
        if (sourceOut.num() > 0 && assetOrNull->duration.num() > 0 &&
            sourceOut > assetOrNull->duration) {
            return Result<void>::fail("clip extends past asset duration (clip " + id + ")");
        }
    }
    if (!assetId.empty() && assetOrNull == nullptr) {
        return Result<void>::fail("clip references missing asset " + assetId);
    }
    return Result<void>::ok();
}

TimeRange Transition::timeRange(const Clip& fromClip, const Clip& /*toClip*/) const {
    const Rational cut = fromClip.seqEnd();
    Rational start = cut;
    switch (alignment) {
    case TransitionAlignment::CenterOnCut:
        start = cut - (duration / 2);
        break;
    case TransitionAlignment::StartOnCut:
        start = cut;
        break;
    case TransitionAlignment::EndOnCut:
        start = cut - duration;
        break;
    }
    if (start.isNegative()) {
        start = Rational(0);
    }
    return TimeRange{start, duration};
}

const Track* Sequence::findTrack(const Id& trackId) const {
    for (const auto& t : tracks) {
        if (t.id == trackId) {
            return &t;
        }
    }
    return nullptr;
}

Track* Sequence::findTrack(const Id& trackId) {
    for (auto& t : tracks) {
        if (t.id == trackId) {
            return &t;
        }
    }
    return nullptr;
}

const Clip* Sequence::findClip(const Id& clipId) const {
    const auto it = clips.find(clipId);
    return it == clips.end() ? nullptr : &it->second;
}

Clip* Sequence::findClip(const Id& clipId) {
    const auto it = clips.find(clipId);
    return it == clips.end() ? nullptr : &it->second;
}

const Transition* Sequence::findTransition(const Id& transId) const {
    for (const auto& tr : transitions) {
        if (tr.id == transId) {
            return &tr;
        }
    }
    return nullptr;
}

Transition* Sequence::findTransition(const Id& transId) {
    for (auto& tr : transitions) {
        if (tr.id == transId) {
            return &tr;
        }
    }
    return nullptr;
}

bool Sequence::removeTransition(const Id& transId) {
    for (auto it = transitions.begin(); it != transitions.end(); ++it) {
        if (it->id == transId) {
            transitions.erase(it);
            return true;
        }
    }
    return false;
}

const Transition* Sequence::findTransitionForClips(const Id& fromClipId, const Id& toClipId) const {
    for (const auto& tr : transitions) {
        if (tr.fromClipId == fromClipId && tr.toClipId == toClipId) {
            return &tr;
        }
    }
    return nullptr;
}

Result<void> Sequence::validate(const std::map<Id, Asset>& assets) const {
    if (id.empty()) {
        return Result<void>::fail("sequence id is empty");
    }
    if (!(fps.num() > 0)) {
        return Result<void>::fail("sequence fps must be positive");
    }
    for (const auto& track : tracks) {
        if (track.id.empty()) {
            return Result<void>::fail("track id is empty");
        }
        for (const auto& clipId : track.clipIds) {
            const auto it = clips.find(clipId);
            if (it == clips.end()) {
                return Result<void>::fail("track " + track.id + " references missing clip " + clipId);
            }
            const Clip& clip = it->second;
            const Asset* asset = nullptr;
            if (!clip.assetId.empty()) {
                const auto ait = assets.find(clip.assetId);
                asset = ait == assets.end() ? nullptr : &ait->second;
                if (asset == nullptr) {
                    return Result<void>::fail("clip " + clip.id + " references missing asset");
                }
            }
            if (const auto r = clip.validate(asset); r.isErr()) {
                return r;
            }
        }
    }
    for (const auto& trans : transitions) {
        if (trans.id.empty()) {
            return Result<void>::fail("transition id is empty");
        }
        if (!(trans.duration.num() > 0)) {
            return Result<void>::fail("transition duration must be positive");
        }
        const Track* tr = findTrack(trans.trackId);
        if (tr == nullptr) {
            return Result<void>::fail("transition " + trans.id + " references missing track " + trans.trackId);
        }
        const Clip* from = findClip(trans.fromClipId);
        const Clip* to = findClip(trans.toClipId);
        if (from == nullptr) {
            return Result<void>::fail("transition " + trans.id + " references missing fromClip " + trans.fromClipId);
        }
        if (to == nullptr) {
            return Result<void>::fail("transition " + trans.id + " references missing toClip " + trans.toClipId);
        }
    }
    return Result<void>::ok();
}

Sequence* Project::activeSequence() {
    for (auto& s : sequences) {
        if (s.id == activeSequenceId) {
            return &s;
        }
    }
    return sequences.empty() ? nullptr : &sequences.front();
}

const Sequence* Project::activeSequence() const {
    for (const auto& s : sequences) {
        if (s.id == activeSequenceId) {
            return &s;
        }
    }
    return sequences.empty() ? nullptr : &sequences.front();
}

Result<void> Project::validate() const {
    if (schemaVersion <= 0) {
        return Result<void>::fail("invalid schema version");
    }
    for (const auto& seq : sequences) {
        if (const auto r = seq.validate(assets); r.isErr()) {
            return r;
        }
    }
    return Result<void>::ok();
}

static double interpolateFactor(double p, const std::string& easing) {
    if (p <= 0.0) return 0.0;
    if (p >= 1.0) return 1.0;
    if (easing == "ease_in") {
        return p * p;
    } else if (easing == "ease_out") {
        return p * (2.0 - p);
    } else if (easing == "ease_in_out") {
        return p * p * (3.0 - 2.0 * p);
    }
    return p; // linear
}

Transform Clip::evaluateTransformAt(const Rational& t) const {
    if (keyframes.empty()) {
        return transform;
    }
    if (t <= keyframes.front().seqTime) {
        return keyframes.front().transform;
    }
    if (t >= keyframes.back().seqTime) {
        return keyframes.back().transform;
    }
    for (std::size_t i = 0; i + 1 < keyframes.size(); ++i) {
        const auto& k0 = keyframes[i];
        const auto& k1 = keyframes[i + 1];
        if (t >= k0.seqTime && t <= k1.seqTime) {
            const Rational span = k1.seqTime - k0.seqTime;
            if (span.num() <= 0) return k0.transform;
            const double rawFactor = static_cast<double>(t - k0.seqTime) / static_cast<double>(span);
            const double factor = interpolateFactor(rawFactor, k0.easing);
            Transform tr;
            tr.scale = k0.transform.scale + (k1.transform.scale - k0.transform.scale) * factor;
            tr.x = k0.transform.x + (k1.transform.x - k0.transform.x) * factor;
            tr.y = k0.transform.y + (k1.transform.y - k0.transform.y) * factor;
            tr.rotationDeg = k0.transform.rotationDeg + (k1.transform.rotationDeg - k0.transform.rotationDeg) * factor;
            return tr;
        }
    }
    return transform;
}

double Clip::evaluateOpacityAt(const Rational& t) const {
    double baseOpacity = opacity;
    if (!keyframes.empty()) {
        if (t <= keyframes.front().seqTime) {
            baseOpacity = keyframes.front().opacity;
        } else if (t >= keyframes.back().seqTime) {
            baseOpacity = keyframes.back().opacity;
        } else {
            for (std::size_t i = 0; i + 1 < keyframes.size(); ++i) {
                const auto& k0 = keyframes[i];
                const auto& k1 = keyframes[i + 1];
                if (t >= k0.seqTime && t <= k1.seqTime) {
                    const Rational span = k1.seqTime - k0.seqTime;
                    if (span.num() > 0) {
                        const double rawFactor = static_cast<double>(t - k0.seqTime) / static_cast<double>(span);
                        const double factor = interpolateFactor(rawFactor, k0.easing);
                        baseOpacity = k0.opacity + (k1.opacity - k0.opacity) * factor;
                    }
                    break;
                }
            }
        }
    }

    double fadeMult = 1.0;
    if (fadeInSec > 0.0) {
        const double offsetSec = static_cast<double>(t - seqStart);
        if (offsetSec < fadeInSec && fadeInSec > 0.0) {
            fadeMult = std::max(0.0, offsetSec / fadeInSec);
        }
    }
    if (fadeOutSec > 0.0) {
        const double remainingSec = static_cast<double>(seqEnd() - t);
        if (remainingSec < fadeOutSec && fadeOutSec > 0.0) {
            fadeMult = std::min(fadeMult, std::max(0.0, remainingSec / fadeOutSec));
        }
    }

    return std::max(0.0, std::min(1.0, baseOpacity * fadeMult));
}

} // namespace editor::core
