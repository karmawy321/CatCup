#pragma once

// Real media prober backed by the pinned FFmpeg (n8.1 gpl-shared).
// Reports canonical Rational timing — never doubles — so probe results can
// feed Asset records directly.

#include "media/MediaInterfaces.hpp"

namespace editor::media_ffmpeg {

class FfmpegProber final : public media::IMediaProber {
public:
    core::Result<media::ProbeResult> probe(const std::string& path) override;
};

} // namespace editor::media_ffmpeg
