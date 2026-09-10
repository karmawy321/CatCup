// Stage 0 headless acceptance driver.
//
// Exercises the full loop without Qt/FFmpeg:
//   editor --create-demo out.json   build 8 s fixture project, 2 cuts, title
//   editor --check in.json          reload + validate + mock-export frame count
//   editor --version                print version
//
// Exit codes: 0 ok, 1 usage error, 2 processing failure.

#include "ai/JobSystem.hpp"
#include "commands/ClipCommands.hpp"
#include "commands/Command.hpp"
#include "core/Ids.hpp"
#include "core/Model.hpp"
#include "effects/EffectSchema.hpp"
#include "export/ExportJob.hpp"
#include "media/MediaInterfaces.hpp"
#include "persist/ProjectSerializer.hpp"
#include "render/Evaluator.hpp"
#ifdef WITH_FFMPEG
#include "export_ffmpeg/Mp4Exporter.hpp"
#include "media_ffmpeg/FfmpegProber.hpp"
#endif

#include <atomic>
#include <cstdio>
#include <iostream>
#include <string>

namespace {

int printUsage() {
    std::cout << "usage:\n"
                 "  editor --version\n"
                 "  editor --create-demo <out.json> [fixture-path]\n"
                 "  editor --check <in.json>\n"
#ifdef WITH_FFMPEG
                 "  editor --export-mp4 <in.json> <out.mp4>\n"
                 "  editor --create-s1 <fixture> <out.json>\n"
#else
                 "  (rebuild with ENABLE_FFMPEG=ON for --export-mp4 / --create-s1)\n"
#endif
                 ;
    return 1;
}

editor::core::Project buildDemo(const std::string& assetPathOrEmpty) {
    using namespace editor;
    core::Project project;
    project.name = "0907-acceptance";

    // Fixture asset: mirrors research/fixtures/inspection-test.mp4
    // (8 s, 1280x720, 30 fps) so the acceptance edit has real timing.
    // An explicit path (e.g. from CI) wins; otherwise the repo-relative
    // default assumes a repo-root working directory.
    media::StubProber prober;
    const std::string fixturePath =
        assetPathOrEmpty.empty() ? "research/fixtures/inspection-test.mp4" : assetPathOrEmpty;
    media::ProbeResult probe = prober.probe(fixturePath).value();

    core::Asset asset;
    asset.id = core::IdGenerator::make("asset");
    asset.kind = core::AssetKind::Video;
    asset.path = fixturePath;
    asset.duration = probe.duration;
    asset.fps = probe.fps;
    asset.width = probe.width;
    asset.height = probe.height;
    project.assets.emplace(asset.id, asset);

    core::Sequence seq;
    seq.id = core::IdGenerator::make("seq");
    seq.name = "Sequence 01";
    seq.fps = core::Rational(30, 1);
    seq.width = 1280;
    seq.height = 720;

    core::Track video;
    video.id = core::IdGenerator::make("track");
    video.kind = core::TrackKind::Video;
    video.name = "V1";
    core::Track text;
    text.id = core::IdGenerator::make("track");
    text.kind = core::TrackKind::Text;
    text.name = "T1";
    const core::Id videoTrackId = video.id;
    const core::Id textTrackId = text.id;
    seq.tracks.push_back(std::move(video));
    seq.tracks.push_back(std::move(text));
    project.sequences.push_back(std::move(seq));
    project.activeSequenceId = project.sequences.front().id;

    commands::UndoStack undo(100);
    std::string error;

    // Full-length clip, then two cuts (split at 2 s and 5 s) via commands.
    core::Clip full;
    full.id = core::IdGenerator::make("clip");
    full.assetId = asset.id;
    full.name = "inspection-test";
    full.sourceIn = core::Rational(0);
    full.sourceOut = core::Rational(8, 1);
    full.seqStart = core::Rational(0);
    const core::Id firstId = full.id;
    if (!undo.execute(commands::makeAddClipCommand(videoTrackId, full), project, error)) {
        throw std::runtime_error("add clip: " + error);
    }
    const core::Id midId = core::IdGenerator::make("clip");
    if (!undo.execute(commands::makeSplitClipCommand(firstId, core::Rational(2, 1), midId),
                      project, error)) {
        throw std::runtime_error("split @2s: " + error);
    }
    const core::Id tailId = core::IdGenerator::make("clip");
    if (!undo.execute(commands::makeSplitClipCommand(midId, core::Rational(5, 1), tailId),
                      project, error)) {
        throw std::runtime_error("split @5s: " + error);
    }

    // Title on the text track (approx 3 s overlay like the observed flow).
    core::Clip title;
    title.id = core::IdGenerator::make("clip");
    title.name = "Title";
    title.sourceIn = core::Rational(0);
    title.sourceOut = core::Rational(3, 1);
    title.seqStart = core::Rational(1, 1);
    title.text = "Hello edit";
    title.fontFamily = "Sans";
    if (!undo.execute(commands::makeAddClipCommand(textTrackId, title), project, error)) {
        throw std::runtime_error("add title: " + error);
    }

    if (auto r = project.validate(); r.isErr()) {
        throw std::runtime_error("demo project invalid: " + r.error());
    }
    return project;
}

int cmdCreateDemo(const std::string& outPath, const std::string& assetPathOrEmpty) {
    try {
        editor::core::Project project = buildDemo(assetPathOrEmpty);
        if (auto r = editor::persist::saveProject(project, outPath); r.isErr()) {
            std::cerr << "save failed: " << r.error() << "\n";
            return 2;
        }
        const auto* seq = project.activeSequence();
        std::cout << "demo project saved: " << outPath << "\n";
        std::cout << "sequence duration: "
                  << editor::render::Evaluator::sequenceDuration(*seq).toString() << "s\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "create-demo failed: " << e.what() << "\n";
        return 2;
    }
}

int cmdCheck(const std::string& inPath) {
    auto loaded = editor::persist::loadProject(inPath);
    if (loaded.isErr()) {
        std::cerr << "load failed: " << loaded.error() << "\n";
        return 2;
    }
    const editor::core::Project& project = loaded.value();
    const editor::core::Sequence* seq = project.activeSequence();
    if (seq == nullptr) {
        std::cerr << "check failed: no sequences\n";
        return 2;
    }
    editor::exporter::ExportSettings settings;
    settings.outPath = "check-mock.mp4";
    editor::exporter::ExportJob job(settings);
    editor::exporter::CountingSink sink;
    if (job.run(project, seq->id, sink) != editor::exporter::ExportState::Done) {
        std::cerr << "mock export failed: " << job.error() << "\n";
        return 2;
    }
    std::cout << "project OK: " << project.name << "\n";
    std::cout << "sequences: " << project.sequences.size()
              << ", assets: " << project.assets.size() << "\n";
    std::cout << "mock export frames: " << sink.frames << " (" << sink.layers
              << " layers evaluated)\n";
    std::cout << "effect schemas: " << editor::effects::EffectRegistry::defaults().all().size()
              << "\n";
    return 0;
}

} // namespace

#ifdef WITH_FFMPEG
namespace s1accept {

// Stage 1 acceptance project: import a real (long) asset, place it on V1+A1,
// make two cuts, add a title — everything through undoable commands, the
// same path the Qt shell drives.
editor::core::Project buildS1(const std::string& fixturePath) {
    using namespace editor;
    media_ffmpeg::FfmpegProber prober;
    auto probed = prober.probe(fixturePath);
    if (probed.isErr()) {
        throw std::runtime_error("probe failed: " + probed.error());
    }
    const media::ProbeResult& pr = probed.value();
    if (!pr.hasVideo) {
        throw std::runtime_error("acceptance fixture has no video: " + fixturePath);
    }

    core::Project project;
    project.name = "s1-acceptance";

    core::Asset asset;
    asset.id = core::IdGenerator::make("asset");
    asset.kind = core::AssetKind::Video;
    asset.path = fixturePath;
    asset.duration = pr.duration;
    asset.fps = pr.fps.num() > 0 ? pr.fps : core::Rational(30, 1);
    asset.width = pr.width;
    asset.height = pr.height;
    project.assets.emplace(asset.id, asset);

    core::Sequence seq;
    seq.id = core::IdGenerator::make("seq");
    seq.name = "Sequence 01";
    seq.fps = core::Rational(30, 1);
    seq.width = pr.width > 0 ? pr.width : 1280;
    seq.height = pr.height > 0 ? pr.height : 720;
    const auto addTrack = [&](core::TrackKind kind, const char* name) {
        core::Track t;
        t.id = core::IdGenerator::make("track");
        t.kind = kind;
        t.name = name;
        seq.tracks.push_back(std::move(t));
    };
    addTrack(core::TrackKind::Video, "V1");
    addTrack(core::TrackKind::Audio, "A1");
    addTrack(core::TrackKind::Text, "T1");
    const core::Id v1 = seq.tracks[0].id;
    const core::Id a1 = seq.tracks[1].id;
    const core::Id t1 = seq.tracks[2].id;
    project.sequences.push_back(std::move(seq));
    project.activeSequenceId = project.sequences.front().id;

    commands::UndoStack undo(100);
    std::string error;
    const auto must = [&](bool ok, const std::string& what) {
        if (!ok) {
            throw std::runtime_error(what + ": " + error);
        }
    };

    core::Clip video;
    video.id = core::IdGenerator::make("clip");
    video.assetId = asset.id;
    video.name = "acceptance";
    video.sourceIn = core::Rational(0);
    video.sourceOut = pr.duration;
    video.seqStart = core::Rational(0);
    const core::Id firstId = video.id;
    must(undo.execute(commands::makeAddClipCommand(v1, video), project, error), "add video");

    if (pr.hasAudio) {
        core::Clip audio = video;
        audio.id = core::IdGenerator::make("clip");
        audio.name = "acceptance-audio";
        must(undo.execute(commands::makeAddClipCommand(a1, audio), project, error),
             "add audio");
    }

    // Two cuts at exact thirds of the asset duration.
    const core::Rational cut1 = pr.duration * core::Rational(1, 3);
    const core::Rational cut2 = pr.duration * core::Rational(2, 3);
    const core::Id midId = core::IdGenerator::make("clip");
    must(undo.execute(commands::makeSplitClipCommand(firstId, cut1, midId), project, error),
         "split #1");
    const core::Id tailId = core::IdGenerator::make("clip");
    must(undo.execute(commands::makeSplitClipCommand(midId, cut2, tailId), project, error),
         "split #2");

    core::Clip title;
    title.id = core::IdGenerator::make("clip");
    title.name = "Title";
    title.sourceIn = core::Rational(0);
    title.sourceOut = core::Rational(3, 1);
    title.seqStart = core::Rational(1, 1);
    title.text = "Stage one";
    title.fontFamily = "Arial";
    title.fontSizePt = 72.0;
    must(undo.execute(commands::makeAddClipCommand(t1, title), project, error), "add title");

    if (auto r = project.validate(); r.isErr()) {
        throw std::runtime_error("s1 project invalid: " + r.error());
    }
    return project;
}

int cmdCreateS1(const std::string& fixture, const std::string& outPath) {
    try {
        editor::core::Project project = buildS1(fixture);
        if (auto r = editor::persist::saveProject(project, outPath); r.isErr()) {
            std::cerr << "save failed: " << r.error() << "\n";
            return 2;
        }
        const auto* seq = project.activeSequence();
        std::cout << "s1 project saved: " << outPath << "\n";
        std::cout << "sequence duration: "
                  << editor::render::Evaluator::sequenceDuration(*seq).toString() << "s\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "create-s1 failed: " << e.what() << "\n";
        return 2;
    }
}

} // namespace s1accept
#endif

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "native_editor 0.1.0 (Stage 0 foundation"
#ifdef WITH_FFMPEG
                  << " + Stage 1 FFmpeg"
#endif
                  << ")\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--create-demo") {
        return cmdCreateDemo(argv[2], {});
    }
    if (argc == 4 && std::string(argv[1]) == "--create-demo") {
        return cmdCreateDemo(argv[2], argv[3]);
    }
    if (argc == 3 && std::string(argv[1]) == "--check") {
        return cmdCheck(argv[2]);
    }
#ifdef WITH_FFMPEG
    if (argc == 4 && std::string(argv[1]) == "--export-mp4") {
        auto loaded = editor::persist::loadProject(argv[2]);
        if (loaded.isErr()) {
            std::cerr << "load failed: " << loaded.error() << "\n";
            return 2;
        }
        const editor::core::Project& project = loaded.value();
        const editor::core::Sequence* seq = project.activeSequence();
        if (seq == nullptr) {
            std::cerr << "export failed: no sequences\n";
            return 2;
        }
        editor::export_ffmpeg::Mp4ExportOptions opts;
        opts.outPath = argv[3];
        std::atomic_bool cancel{false};
        editor::export_ffmpeg::Mp4Exporter exporter;
        auto r = exporter.run(project, seq->id, opts, cancel, [](double p) {
            std::cout << "\rprogress " << static_cast<int>(p * 100) << "%" << std::flush;
        });
        std::cout << "\n";
        if (r.isErr()) {
            std::cerr << "export failed: " << r.error() << "\n";
            return 2;
        }
        std::cout << "exported: " << argv[3] << "\n";
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--create-s1") {
        return s1accept::cmdCreateS1(argv[2], argv[3]);
    }
#endif
    return printUsage();
}
