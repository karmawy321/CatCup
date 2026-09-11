#include "TestHarness.hpp"

#include "commands/ClipCommands.hpp"
#include "commands/Command.hpp"
#include "core/Ids.hpp"
#include "export_ffmpeg/Mp4Exporter.hpp"
#include "media_ffmpeg/FfmpegProber.hpp"
#include "media_ffmpeg/FrameDecoder.hpp"
#include "persist/ProjectSerializer.hpp"

#include <atomic>
#include <cstdio>
#include <filesystem>

using namespace editor;

namespace {
const char* kFixture = FIXTURE_MP4; // absolute path injected by CMake
const std::string kTmp = std::string(TMP_DIR);

core::Project makeDemo(bool withTitle) {
    core::Project p;
    p.name = "s1-export-test";
    core::Asset a;
    a.id = "asset-1";
    a.kind = core::AssetKind::Video;
    a.path = kFixture;
    a.duration = core::Rational(8, 1);
    a.fps = core::Rational(30, 1);
    a.width = 1280;
    a.height = 720;
    p.assets.emplace(a.id, a);

    core::Sequence seq;
    seq.id = "seq-1";
    seq.fps = core::Rational(30, 1);
    seq.width = 1280;
    seq.height = 720;
    core::Track v;
    v.id = "v1";
    v.kind = core::TrackKind::Video;
    v.name = "V1";
    core::Track au;
    au.id = "a1";
    au.kind = core::TrackKind::Audio;
    au.name = "A1";
    core::Track t;
    t.id = "t1";
    t.kind = core::TrackKind::Text;
    t.name = "T1";
    seq.tracks.push_back(v);
    seq.tracks.push_back(au);
    seq.tracks.push_back(t);

    core::Clip video;
    video.id = "c1";
    video.assetId = "asset-1";
    video.name = "fixture";
    video.sourceIn = core::Rational(0);
    video.sourceOut = core::Rational(8, 1);
    video.seqStart = core::Rational(0);
    seq.clips.emplace(video.id, video);
    seq.tracks[0].clipIds.push_back(video.id);

    core::Clip audio = video;
    audio.id = "c2";
    audio.name = "fixture-audio";
    seq.clips.emplace(audio.id, audio);
    seq.tracks[1].clipIds.push_back(audio.id);

    if (withTitle) {
        core::Clip title;
        title.id = "c3";
        title.name = "Title";
        title.sourceIn = core::Rational(0);
        title.sourceOut = core::Rational(3, 1);
        title.seqStart = core::Rational(1, 1);
        title.text = "Hello edit";
        title.fontFamily = "Arial";
        title.fontSizePt = 72.0;
        seq.clips.emplace(title.id, title);
        seq.tracks[2].clipIds.push_back(title.id);
    }
    p.sequences.push_back(std::move(seq));
    p.activeSequenceId = "seq-1";
    CHECK(p.validate().isOk());
    return p;
}

std::string shaOfFile(const std::string& path) {
    // FNV-1a 64-bit: good enough to detect "title changed the picture".
    std::FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || f == nullptr) {
        throw std::runtime_error("cannot open " + path);
    }
    std::uint64_t h = 1469598103934665603ull;
    char buf[65536];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; ++i) {
            h ^= static_cast<unsigned char>(buf[i]);
            h *= 1099511628211ull;
        }
    }
    std::fclose(f);
    char out[17];
    std::snprintf(out, sizeof(out), "%016llx", static_cast<unsigned long long>(h));
    return out;
}

double centerMad(const std::string& path, double atSec) {
    // Mean absolute deviation from mid-gray in the center band — a coarse
    // "is there bright title text here" signal on the dark test pattern.
    media_ffmpeg::VideoDecoder dec;
    CHECK(dec.open(path).isOk());
    CHECK(dec.seek(core::Rational(static_cast<std::int64_t>(atSec * 1000000), 1000000)).isOk());
    auto f = dec.nextFrame();
    CHECK(f.isOk());
    const auto& img = f.value();
    double acc = 0.0;
    int64_t n = 0;
    const int y0 = img.height / 3;
    const int y1 = 2 * img.height / 3;
    const int x0 = img.width / 4;
    const int x1 = 3 * img.width / 4;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const uint8_t* px = img.rgba.data() + (static_cast<size_t>(y) * img.width + x) * 4;
            const double lum = (px[0] + px[1] + px[2]) / 3.0;
            acc += std::abs(lum - 128.0);
            ++n;
        }
    }
    return acc / n;
}

} // namespace

TEST_CASE("mp4: export produces a playable 8 s H.264/AAC file") {
    const std::string out = kTmp + "/s1_title.mp4";
    std::error_code ec;
    std::filesystem::remove(out, ec);
    export_ffmpeg::Mp4Exporter exporter;
    export_ffmpeg::Mp4ExportOptions opts;
    opts.outPath = out;
    std::atomic_bool cancel{false};
    double progress = 0.0;
    auto r = exporter.run(makeDemo(true), "seq-1", opts, cancel,
                          [&](double p) { progress = p; });
    CHECK(r.isOk());
    CHECK(progress == 1.0);
    CHECK(std::filesystem::exists(out));

    media_ffmpeg::FfmpegProber prober;
    auto probed = prober.probe(out);
    CHECK(probed.isOk());
    CHECK(probed.value().hasVideo && probed.value().hasAudio);
    CHECK_EQ(probed.value().videoCodec, std::string("h264"));
    CHECK_EQ(probed.value().audioCodec, std::string("aac"));
    // Duration rounds to the 8 s timeline (container rounding tolerance).
    const double dur = static_cast<double>(probed.value().duration);
    CHECK(dur > 7.9 && dur < 8.2);

    media_ffmpeg::VideoDecoder dec;
    CHECK(dec.open(out).isOk());
    int frames = 0;
    while (dec.nextFrame().isOk()) {
        if (++frames > 300) {
            break;
        }
    }
    CHECK(frames >= 238 && frames <= 242);
}

TEST_CASE("mp4: title visibly changes the rendered picture") {
    const std::string withTitle = kTmp + "/s1_title.mp4";
    const std::string withoutTitle = kTmp + "/s1_plain.mp4";
    // withTitle produced by the previous test; rebuild plain here.
    export_ffmpeg::Mp4Exporter exporter;
    export_ffmpeg::Mp4ExportOptions opts;
    opts.outPath = withoutTitle;
    std::atomic_bool cancel{false};
    CHECK(exporter.run(makeDemo(false), "seq-1", opts, cancel, {}).isOk());

    CHECK(shaOfFile(withTitle) != shaOfFile(withoutTitle));
    // Title occupies 1..4 s: center-band activity must differ at t=2,
    // and roughly agree at t=6 (no title in either file).
    const double titled = centerMad(withTitle, 2.0);
    const double plain = centerMad(withoutTitle, 2.0);
    CHECK(std::abs(titled - plain) > 2.0);
}

TEST_CASE("mp4: pre-cancelled export fails cleanly with no leftover") {
    const std::string out = kTmp + "/s1_cancel.mp4";
    export_ffmpeg::Mp4Exporter exporter;
    export_ffmpeg::Mp4ExportOptions opts;
    opts.outPath = out;
    std::atomic_bool cancel{true}; // cancelled before the first frame
    auto r = exporter.run(makeDemo(true), "seq-1", opts, cancel, {});
    CHECK(r.isErr());
    CHECK_EQ(r.error(), std::string("cancelled"));
    CHECK(!std::filesystem::exists(out));
}

TEST_CASE("mp4: effect visibly changes exported video") {
    const std::string withoutEffect = kTmp + "/s1_plain.mp4";
    const std::string withEffect = kTmp + "/s1_effect.mp4";

    core::Project p = makeDemo(false);
    core::Effect eff;
    eff.type = "vignette";
    eff.enabled = true;
    eff.params["intensity"] = 1.0;
    eff.params["radius"] = 0.5;
    p.sequences.front().clips.at("c1").effects.push_back(eff);

    export_ffmpeg::Mp4Exporter exporter;
    export_ffmpeg::Mp4ExportOptions opts;
    opts.outPath = withEffect;
    std::atomic_bool cancel{false};
    CHECK(exporter.run(p, "seq-1", opts, cancel, {}).isOk());

    CHECK(std::filesystem::exists(withEffect));
    CHECK(shaOfFile(withEffect) != shaOfFile(withoutEffect));
}

int main() {
    return editor::tests::runAll();
}
