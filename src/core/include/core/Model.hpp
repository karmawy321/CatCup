#pragma once

// Project model: assets, sequences, tracks, clips, effects.
// Plain data + validated helpers. No I/O, no UI, no media decoding here —
// persistence lives in persist/, edits go through commands/.

#include "core/Ids.hpp"
#include "core/Rational.hpp"
#include "core/Result.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace editor::core {

inline constexpr int kCurrentSchemaVersion = 2;

enum class AssetKind { Video, Audio, Image, Text };
enum class TrackKind { Video, Audio, Text };
enum class TransitionAlignment { CenterOnCut, StartOnCut, EndOnCut };

[[nodiscard]] const char* toString(AssetKind k) noexcept;
[[nodiscard]] const char* toString(TrackKind k) noexcept;
[[nodiscard]] const char* toString(TransitionAlignment a) noexcept;
Result<AssetKind> assetKindFromString(const std::string& s);
Result<TrackKind> trackKindFromString(const std::string& s);
Result<TransitionAlignment> transitionAlignmentFromString(const std::string& s);

struct Asset {
    Id id;
    AssetKind kind = AssetKind::Video;
    std::string path;            // as imported; relink resolved by persist/
    Rational duration{0};        // canonical source length
    Rational fps{30};            // nominal rate; VFR handled in Stage 2+
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::string hash;            // content hash when known (derived assets)
};

struct Effect {
    std::string type;
    int version = 1;
    bool enabled = true;
    int order = 0; // evaluation order within the clip stack
    std::map<std::string, double> params;
    std::map<std::string, std::string> strParams;
};

struct Transition {
    Id id;
    std::string type = "crossfade";
    Id trackId;
    Id fromClipId;
    Id toClipId;
    Rational duration{1, 1}; // canonical length in seconds
    TransitionAlignment alignment = TransitionAlignment::CenterOnCut;
    std::string easing = "linear";
    std::map<std::string, double> params;

    [[nodiscard]] TimeRange timeRange(const struct Clip& fromClip, const struct Clip& toClip) const;
};

struct Transform {
    double scale = 1.0;
    double x = 0.0;
    double y = 0.0;
    double rotationDeg = 0.0;
};

struct Clip {
    Id id;
    Id assetId;       // empty for generated text clips
    std::string name; // display label, e.g. file stem or "Title"
    Rational sourceIn{0};
    Rational sourceOut{0}; // exclusive; must satisfy sourceIn < sourceOut
    Rational seqStart{0};  // placement on the sequence timeline
    bool enabled = true;
    double opacity = 1.0;
    Transform transform{};
    std::vector<Effect> effects;
    // Text payload (Stage 1 title). Stays here so preview/export share it.
    std::string text;
    std::string fontFamily;
    double fontSizePt = 48.0;

    [[nodiscard]] Rational seqDuration() const { return sourceOut - sourceIn; }
    [[nodiscard]] Rational seqEnd() const { return seqStart + seqDuration(); }
    [[nodiscard]] TimeRange seqRange() const { return TimeRange{seqStart, seqDuration()}; }
    /// Map a sequence time to source time (speed == 1 in Stage 0/1).
    [[nodiscard]] Rational mapToSource(const Rational& seqTime) const {
        return sourceIn + (seqTime - seqStart);
    }
    [[nodiscard]] Result<void> validate(const Asset* assetOrNull) const;
};

struct Track {
    Id id;
    TrackKind kind = TrackKind::Video;
    std::string name;
    bool locked = false;
    bool visible = true; // video/text
    bool muted = false;  // audio
    std::vector<Id> clipIds; // z-order = vector order (last = top)
};

struct Sequence {
    Id id;
    std::string name = "Sequence 01";
    Rational fps{30};
    std::int64_t width = 1280;
    std::int64_t height = 720;
    std::vector<Track> tracks;
    std::map<Id, Clip> clips; // clip id -> clip (placement lives here)
    std::vector<Transition> transitions;

    [[nodiscard]] const Track* findTrack(const Id& trackId) const;
    Track* findTrack(const Id& trackId);
    [[nodiscard]] const Clip* findClip(const Id& clipId) const;
    Clip* findClip(const Id& clipId);
    [[nodiscard]] const Transition* findTransition(const Id& transId) const;
    Transition* findTransition(const Id& transId);
    bool removeTransition(const Id& transId);
    [[nodiscard]] const Transition* findTransitionForClips(const Id& fromClipId, const Id& toClipId) const;
    [[nodiscard]] Result<void> validate(const std::map<Id, Asset>& assets) const;
};

struct Project {
    int schemaVersion = kCurrentSchemaVersion;
    std::string name = "Untitled";
    std::map<Id, Asset> assets;
    std::vector<Sequence> sequences;
    Id activeSequenceId;

    [[nodiscard]] Sequence* activeSequence();
    [[nodiscard]] const Sequence* activeSequence() const;
    [[nodiscard]] Result<void> validate() const;
};

} // namespace editor::core
