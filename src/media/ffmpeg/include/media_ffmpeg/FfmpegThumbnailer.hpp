#pragma once

// Filmstrip thumbnails via the real decoder: seek, grab, scale, return RGBA.
// The async cache and QML image provider live in the shell (Stage 1); this
// stays synchronous and UI-free so export/tests can use it too.

#include "core/Rational.hpp"
#include "core/Result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace editor::media_ffmpeg {

struct ThumbnailImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

class FfmpegThumbnailer {
public:
    core::Result<void> open(const std::string& path);
    core::Result<ThumbnailImage> grab(const core::Rational& at, int targetWidth = 192);

private:
    std::string path_;
    bool open_ = false;
};

} // namespace editor::media_ffmpeg
