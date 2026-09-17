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

TEST_CASE("ffmpeg: video decoder multi-directional seek and near-EOF navigation") {
    media_ffmpeg::VideoDecoder dec;
    CHECK(dec.open(kFixture).isOk());

    // 1. Seek forward to 6s
    CHECK(dec.seek(core::Rational(6, 1)).isOk());
    auto f1 = dec.nextFrame();
    CHECK(f1.isOk());
    double pts1 = static_cast<double>(f1.value().pts);
    CHECK(pts1 >= 5.9 && pts1 <= 6.1);

    // 2. Seek backward to 1s
    CHECK(dec.seek(core::Rational(1, 1)).isOk());
    auto f2 = dec.nextFrame();
    CHECK(f2.isOk());
    double pts2 = static_cast<double>(f2.value().pts);
    CHECK(pts2 >= 0.9 && pts2 <= 1.1);

    // 3. Seek forward to 4s
    CHECK(dec.seek(core::Rational(4, 1)).isOk());
    auto f3 = dec.nextFrame();
    CHECK(f3.isOk());
    double pts3 = static_cast<double>(f3.value().pts);
    CHECK(pts3 >= 3.9 && pts3 <= 4.1);

    // 4. Seek backward to 0s
    CHECK(dec.seek(core::Rational(0, 1)).isOk());
    auto f4 = dec.nextFrame();
    CHECK(f4.isOk());
    double pts4 = static_cast<double>(f4.value().pts);
    CHECK(pts4 >= 0.0 && pts4 <= 0.05);

    // 5. Seek near EOF (7.9s on 8s fixture) -> must return valid frame and not crash
    CHECK(dec.seek(core::Rational(79, 10)).isOk());
    auto f5 = dec.nextFrame();
    CHECK(f5.isOk());
    CHECK(f5.value().width == 1280);
    CHECK(f5.value().height == 720);

    // 6. After EOF/near EOF, seeking back to 2s must immediately work and recover from EOF
    CHECK(dec.seek(core::Rational(2, 1)).isOk());
    auto f6 = dec.nextFrame();
    CHECK(f6.isOk());
    double pts6 = static_cast<double>(f6.value().pts);
    CHECK(pts6 >= 1.9 && pts6 <= 2.1);
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
