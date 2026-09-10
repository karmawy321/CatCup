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

Result<void> Clip::validate(const Asset* assetOrNull) const {
    if (id.empty()) {
        return Result<void>::fail("clip id is empty");
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

} // namespace editor::core
