#pragma once

// Media interfaces. core/ and render/ program against these — never against
// libav* directly. Only src/media/ffmpeg/ (Stage 1, ENABLE_FFMPEG) may
// include FFmpeg headers, so swapping or stubbing the backend cannot leak
// into the domain model.

#include "core/Ids.hpp"
#include "core/Rational.hpp"
#include "core/Result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace editor::media {

struct ProbeResult {
    core::Rational duration{0};
    core::Rational fps{30};
    std::int64_t width = 0;
    std::int64_t height = 0;
    bool hasVideo = false;
    bool hasAudio = false;
    std::string videoCodec;
    std::string audioCodec;
};

class IMediaProber {
public:
    virtual ~IMediaProber() = default;
    [[nodiscard]] virtual core::Result<ProbeResult> probe(const std::string& path) = 0;
};

struct Thumbnail {
    core::Id assetId;
    core::Rational at{0};
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::vector<std::uint8_t> rgba; // width*height*4, empty when unavailable
};

class IThumbnailProvider {
public:
    virtual ~IThumbnailProvider() = default;
    [[nodiscard]] virtual core::Result<Thumbnail> thumbnail(const core::Id& assetId,
                                                           const core::Rational& at) = 0;
};

/// Deterministic stub for Stage 0 tests and headless runs: knows the
/// synthetic inspection fixture (8 s, 1280x720, 30 fps) and reports
/// "unknown file" for anything else so tests cannot silently pass on
/// unprobed media.
class StubProber final : public IMediaProber {
public:
    core::Result<ProbeResult> probe(const std::string& path) override;
};

} // namespace editor::media
