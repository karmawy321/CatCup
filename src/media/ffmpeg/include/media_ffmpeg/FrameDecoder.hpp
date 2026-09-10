#pragma once

// Single-threaded, caller-driven decoders. The caller owns threading:
// preview drives these from a worker with a seek-generation guard, export
// drives them synchronously. No internal threads here by design (S2 may add
// a decode-ahead pool behind this same interface).
//
// Canonical output (one path for preview AND export):
//   video -> RGBA pixels, source dimensions
//   audio -> s16 interleaved, stereo, 48000 Hz
// Resampling/normalization inside the decoder keeps every consumer honest.

#include "core/Rational.hpp"
#include "core/Result.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace editor::media_ffmpeg {

struct DecodedVideoFrame {
    int width = 0;
    int height = 0;
    core::Rational pts{0}; // presentation time in seconds (source timeline)
    std::vector<std::uint8_t> rgba;
};

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    core::Result<void> open(const std::string& path);
    /// Next call to nextFrame() returns the first frame with pts >= t.
    core::Result<void> seek(const core::Rational& t);
    /// Fails with error "eof" at end of stream (not a malfunction).
    core::Result<DecodedVideoFrame> nextFrame();

    [[nodiscard]] bool isOpen() const noexcept { return open_; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] core::Rational fps() const noexcept { return fps_; }
    [[nodiscard]] core::Rational duration() const noexcept { return duration_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool open_ = false;
    int width_ = 0;
    int height_ = 0;
    core::Rational fps_{30};
    core::Rational duration_{0};
};

struct DecodedAudioChunk {
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 2;
    core::Rational startPts{0}; // seconds, source timeline
    std::vector<std::int16_t> pcm; // interleaved s16 stereo @48k
};

class AudioDecoder {
public:
    AudioDecoder();
    ~AudioDecoder();
    AudioDecoder(const AudioDecoder&) = delete;
    AudioDecoder& operator=(const AudioDecoder&) = delete;

    core::Result<void> open(const std::string& path);
    [[nodiscard]] bool hasAudio() const noexcept { return hasAudio_; }
    core::Result<void> seek(const core::Rational& t);
    /// Up to maxSamples per channel; short/empty pcm with ok() == true means
    /// drained tail. Fails with "eof" only when nothing remains at all.
    core::Result<DecodedAudioChunk> nextChunk(int maxSamplesPerChannel);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool hasAudio_ = false;
};

} // namespace editor::media_ffmpeg
