#include "TestHarness.hpp"

#include "media_ffmpeg/FfmpegProber.hpp"
#include "media_ffmpeg/FfmpegThumbnailer.hpp"
#include "media_ffmpeg/FrameDecoder.hpp"

#include <cmath>

using namespace editor;

namespace {
const char* kFixture = FIXTURE_MP4; // absolute path injected by CMake
}

TEST_CASE("ffmpeg: probe reports the synthetic fixture exactly") {
    media_ffmpeg::FfmpegProber prober;
    auto r = prober.probe(kFixture);
    CHECK(r.isOk());
    const media::ProbeResult& p = r.value();
    CHECK(p.hasVideo && p.hasAudio);
    CHECK_EQ(p.duration, core::Rational(8, 1));
    CHECK_EQ(p.fps, core::Rational(30, 1));
    CHECK_EQ(p.width, 1280);
    CHECK_EQ(p.height, 720);
    CHECK_EQ(p.videoCodec, std::string("mpeg4"));
    CHECK_EQ(p.audioCodec, std::string("aac"));
}

TEST_CASE("ffmpeg: unknown file fails with a useful message") {
    media_ffmpeg::FfmpegProber prober;
    auto r = prober.probe("definitely-not-a-file-xyz.mp4");
    CHECK(r.isErr());
    CHECK(!r.error().empty());
}

TEST_CASE("ffmpeg: video decodes 240 frames with exact timestamps") {
    media_ffmpeg::VideoDecoder dec;
    CHECK(dec.open(kFixture).isOk());
    int frames = 0;
    core::Rational firstPts{-1, 1};
    core::Rational lastPts{0};
    while (true) {
        auto f = dec.nextFrame();
        if (f.isErr()) {
            CHECK_EQ(f.error(), std::string("eof"));
            break;
        }
        if (frames == 0) {
            firstPts = f.value().pts;
        }
        lastPts = f.value().pts;
        CHECK_EQ(f.value().width, 1280);
        CHECK_EQ(f.value().height, 720);
        CHECK_EQ(static_cast<int>(f.value().rgba.size()), 1280 * 720 * 4);
        ++frames;
        if (frames > 300) {
            throw std::runtime_error("decoder produced too many frames");
        }
    }
    CHECK_EQ(frames, 240);
    CHECK_EQ(firstPts, core::Rational(0));
    // Last frame at 239/30 s; duration 8 s.
    CHECK_EQ(lastPts, core::Rational(239, 30));
}

TEST_CASE("ffmpeg: seek lands on the first frame at or past target") {
    media_ffmpeg::VideoDecoder dec;
    CHECK(dec.open(kFixture).isOk());
    CHECK(dec.seek(core::Rational(5, 1)).isOk());
    auto f = dec.nextFrame();
    CHECK(f.isOk());
    // mpeg4 baseline: exact frame; allow decoders with B-frame delay one frame.
    const double pts = static_cast<double>(f.value().pts);
    CHECK(pts >= 5.0 - 1e-6 && pts <= 5.0 + 1.0 / 30 + 1e-6);
}

TEST_CASE("ffmpeg: audio normalizes to s16 stereo 48k totaling ~8 s") {
    media_ffmpeg::AudioDecoder dec;
    CHECK(dec.open(kFixture).isOk());
    CHECK(dec.hasAudio());
    int64_t total = 0;
    while (true) {
        auto c = dec.nextChunk(4096);
        if (c.isErr()) {
            CHECK_EQ(c.error(), std::string("eof"));
            break;
        }
        total += static_cast<int64_t>(c.value().pcm.size() / 2);
        if (total > 48000 * 10) {
            throw std::runtime_error("decoder produced too much audio");
        }
    }
    // 8 s * 48000 = 384000 samples/channel; allow codec padding/delay.
    CHECK(total >= 384000 - 2048 && total <= 384000 + 4096);
}

TEST_CASE("ffmpeg: thumbnail grabs a 192-wide still") {
    media_ffmpeg::FfmpegThumbnailer th;
    CHECK(th.open(kFixture).isOk());
    auto img = th.grab(core::Rational(2, 1), 192);
    CHECK(img.isOk());
    CHECK_EQ(img.value().width, 192);
    CHECK(img.value().height > 0);
    CHECK_EQ(static_cast<int>(img.value().rgba.size()),
             img.value().width * img.value().height * 4);
}

int main() {
    return editor::tests::runAll();
}
