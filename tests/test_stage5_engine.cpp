#include "TestHarness.hpp"

#include "ai/AudioAnalysis.hpp"
#include "commands/ClipCommands.hpp"
#include "commands/Command.hpp"
#include "core/Model.hpp"
#include "persist/ProjectSerializer.hpp"
#include "render/Evaluator.hpp"

#include <cmath>
#include <vector>

using namespace editor;

static void checkNear(double a, double b, double eps = 0.01) {
    CHECK(std::abs(a - b) <= eps);
}

TEST_CASE("stage5: keyframe transform and opacity interpolation") {
    core::Clip clip;
    clip.id = "clip-kf";
    clip.seqStart = core::Rational(0);
    clip.sourceIn = core::Rational(0);
    clip.sourceOut = core::Rational(10);
    clip.transform.scale = 1.0;
    clip.transform.x = 0.0;
    clip.transform.y = 0.0;
    clip.transform.rotationDeg = 0.0;
    clip.opacity = 1.0;

    // No keyframes -> returns base transform/opacity
    auto tr0 = clip.evaluateTransformAt(core::Rational(5));
    CHECK_EQ(tr0.scale, 1.0);
    CHECK_EQ(clip.evaluateOpacityAt(core::Rational(5)), 1.0);

    // Add 2 keyframes:
    // At t=2s: scale 1.0, x=0, rot=0, opacity=0.0
    // At t=6s: scale 3.0, x=200, rot=90, opacity=1.0
    core::Keyframe k0;
    k0.seqTime = core::Rational(2);
    k0.transform.scale = 1.0;
    k0.transform.x = 0.0;
    k0.transform.y = 0.0;
    k0.transform.rotationDeg = 0.0;
    k0.opacity = 0.0;
    k0.easing = "linear";

    core::Keyframe k1;
    k1.seqTime = core::Rational(6);
    k1.transform.scale = 3.0;
    k1.transform.x = 200.0;
    k1.transform.y = 100.0;
    k1.transform.rotationDeg = 90.0;
    k1.opacity = 1.0;
    k1.easing = "linear";

    clip.keyframes.push_back(k0);
    clip.keyframes.push_back(k1);

    // At t <= 2s -> matches k0 exactly
    auto trBefore = clip.evaluateTransformAt(core::Rational(1));
    CHECK_EQ(trBefore.scale, 1.0);
    CHECK_EQ(trBefore.x, 0.0);
    CHECK_EQ(clip.evaluateOpacityAt(core::Rational(1)), 0.0);

    // At t >= 6s -> matches k1 exactly
    auto trAfter = clip.evaluateTransformAt(core::Rational(8));
    CHECK_EQ(trAfter.scale, 3.0);
    CHECK_EQ(trAfter.x, 200.0);
    CHECK_EQ(trAfter.rotationDeg, 90.0);
    CHECK_EQ(clip.evaluateOpacityAt(core::Rational(8)), 1.0);

    // Midpoint at t=4s (50% progress):
    // scale = 1.0 + (3.0 - 1.0) * 0.5 = 2.0
    // x = 100.0, y = 50.0, rot = 45.0, opacity = 0.5
    auto trMid = clip.evaluateTransformAt(core::Rational(4));
    checkNear(trMid.scale, 2.0, 0.001);
    checkNear(trMid.x, 100.0, 0.001);
    checkNear(trMid.y, 50.0, 0.001);
    checkNear(trMid.rotationDeg, 45.0, 0.001);
    checkNear(clip.evaluateOpacityAt(core::Rational(4)), 0.5, 0.001);
}

TEST_CASE("stage5: clip audio/video fade in and fade out") {
    core::Clip clip;
    clip.id = "clip-fade";
    clip.seqStart = core::Rational(0);
    clip.sourceIn = core::Rational(0);
    clip.sourceOut = core::Rational(10);
    clip.opacity = 1.0;
    clip.fadeInSec = 2.0;  // 2s fade in
    clip.fadeOutSec = 2.0; // 2s fade out (at 8s..10s)

    // At t=0s -> opacity = 0.0
    checkNear(clip.evaluateOpacityAt(core::Rational(0)), 0.0, 0.001);
    // At t=1s -> opacity = 0.5 (halfway through 2s fade in)
    checkNear(clip.evaluateOpacityAt(core::Rational(1)), 0.5, 0.001);
    // At t=5s -> opacity = 1.0 (full opacity in middle)
    checkNear(clip.evaluateOpacityAt(core::Rational(5)), 1.0, 0.001);
    // At t=9s -> opacity = 0.5 (halfway through 2s fade out)
    checkNear(clip.evaluateOpacityAt(core::Rational(9)), 0.5, 0.001);
    // At t=10s -> opacity = 0.0
    checkNear(clip.evaluateOpacityAt(core::Rational(10)), 0.0, 0.001);
}

TEST_CASE("stage5: audio lufs and normalization") {
    const int sampleRate = 48000;
    const int channels = 2;
    std::vector<float> pcm(sampleRate * 2 * channels, 0.0f);

    // Generate 1 kHz sine wave at amplitude 0.2
    for (std::size_t i = 0; i < pcm.size(); ++i) {
        pcm[i] = 0.2f * std::sin(2.0f * 3.14159265f * 1000.0f * (static_cast<float>(i) / 48000.0f));
    }

    double peakDb = ai::AudioAnalysis::calculatePeakDb(pcm.data(), pcm.size());
    checkNear(peakDb, 20.0 * std::log10(0.2), 0.5);

    double lufs = ai::AudioAnalysis::calculateIntegratedLufs(pcm.data(), pcm.size(), sampleRate, channels);
    CHECK(lufs < -10.0 && lufs > -30.0);

    // Normalize to -14 LUFS
    ai::AudioAnalysis::normalizeLoudness(pcm.data(), pcm.size(), lufs, -14.0, -1.0);

    double normPeak = ai::AudioAnalysis::calculatePeakDb(pcm.data(), pcm.size());
    CHECK(normPeak <= -0.9); // safely under -1.0 dBTP ceiling
}

TEST_CASE("stage5: audio voice cleanup rumble filter") {
    const int sampleRate = 48000;
    std::vector<float> pcm(sampleRate, 0.0f);

    // Very low 20 Hz rumble noise
    for (std::size_t i = 0; i < pcm.size(); ++i) {
        pcm[i] = 0.5f * std::sin(2.0f * 3.14159265f * 20.0f * (static_cast<float>(i) / 48000.0f));
    }

    double beforePeak = ai::AudioAnalysis::calculatePeakDb(pcm.data(), pcm.size());
    checkNear(beforePeak, 20.0 * std::log10(0.5), 0.5);

    // Apply 80 Hz high-pass rumble filter
    ai::AudioAnalysis::applyVoiceCleanup(pcm.data(), pcm.size(), sampleRate, 1, 80.0);

    double afterPeak = ai::AudioAnalysis::calculatePeakDb(pcm.data() + sampleRate / 4, sampleRate * 3 / 4);
    CHECK(afterPeak < beforePeak - 6.0); // significantly attenuated
}

TEST_CASE("stage5: keyframe and format commands with undo/redo") {
    core::Project project;
    core::Sequence seq;
    seq.id = "seq1";
    seq.width = 1920;
    seq.height = 1080;

    core::Track track;
    track.id = "v1";
    track.kind = core::TrackKind::Video;
    track.clipIds.push_back("clip1");

    core::Clip clip;
    clip.id = "clip1";
    clip.seqStart = core::Rational(0);
    clip.sourceIn = core::Rational(0);
    clip.sourceOut = core::Rational(5);

    seq.clips.emplace(clip.id, clip);
    seq.tracks.push_back(std::move(track));
    project.sequences.push_back(std::move(seq));
    project.activeSequenceId = "seq1";

    commands::UndoStack stack;
    std::string err;

    // 1. Change sequence format to vertical 9:16 (1080x1920)
    auto fmtCmd = commands::makeSetSequenceFormatCommand(1080, 1920);
    CHECK(stack.execute(std::move(fmtCmd), project, err));
    CHECK_EQ(project.sequences[0].width, 1080);
    CHECK_EQ(project.sequences[0].height, 1920);

    stack.undo(project);
    CHECK_EQ(project.sequences[0].width, 1920);
    CHECK_EQ(project.sequences[0].height, 1080);

    stack.redo(project, err);
    CHECK_EQ(project.sequences[0].width, 1080);
    CHECK_EQ(project.sequences[0].height, 1920);

    // 2. Set keyframe on clip
    core::Keyframe kf;
    kf.seqTime = core::Rational(2);
    kf.transform.scale = 2.5;
    kf.transform.x = 50.0;
    auto kfCmd = commands::makeSetKeyframeCommand("clip1", kf);
    CHECK(stack.execute(std::move(kfCmd), project, err));
    CHECK_EQ(project.sequences[0].clips["clip1"].keyframes.size(), 1);
    CHECK_EQ(project.sequences[0].clips["clip1"].keyframes[0].transform.scale, 2.5);

    // 3. Set fade
    auto fadeCmd = commands::makeSetFadeCommand("clip1", 1.5, 1.0);
    CHECK(stack.execute(std::move(fadeCmd), project, err));
    CHECK_EQ(project.sequences[0].clips["clip1"].fadeInSec, 1.5);
    CHECK_EQ(project.sequences[0].clips["clip1"].fadeOutSec, 1.0);

    stack.undo(project);
    CHECK_EQ(project.sequences[0].clips["clip1"].fadeInSec, 0.0);

    // 4. Remove keyframe
    stack.redo(project, err);
    auto remKfCmd = commands::makeRemoveKeyframeCommand("clip1", core::Rational(2));
    CHECK(stack.execute(std::move(remKfCmd), project, err));
    CHECK_EQ(project.sequences[0].clips["clip1"].keyframes.size(), 0);

    stack.undo(project);
    CHECK_EQ(project.sequences[0].clips["clip1"].keyframes.size(), 1);
}

TEST_CASE("stage5: project serialization with keyframes and fades") {
    core::Project project;
    core::Sequence seq;
    seq.id = "seq-s5";
    seq.width = 1080;
    seq.height = 1920;

    core::Track track;
    track.id = "v1";
    track.kind = core::TrackKind::Video;
    track.clipIds.push_back("clip-s5");

    core::Clip clip;
    clip.id = "clip-s5";
    clip.seqStart = core::Rational(0);
    clip.sourceIn = core::Rational(0);
    clip.sourceOut = core::Rational(10);
    clip.fadeInSec = 1.25;
    clip.fadeOutSec = 2.50;

    core::Keyframe kf1;
    kf1.seqTime = core::Rational(1);
    kf1.transform.scale = 1.5;
    kf1.transform.x = 25.0;
    kf1.opacity = 0.8;
    kf1.easing = "ease_in_out";
    clip.keyframes.push_back(kf1);

    seq.clips.emplace(clip.id, clip);
    seq.tracks.push_back(std::move(track));
    project.sequences.push_back(std::move(seq));
    project.activeSequenceId = "seq-s5";

    // Serialize
    auto jsonRes = persist::projectToJson(project);
    CHECK(jsonRes.isOk());

    // Deserialize
    auto loadedRes = persist::projectFromJson(jsonRes.value());
    CHECK(loadedRes.isOk());

    const auto& loaded = loadedRes.value();
    CHECK_EQ(loaded.sequences[0].width, 1080);
    CHECK_EQ(loaded.sequences[0].height, 1920);

    const auto& loadedClip = loaded.sequences[0].clips.at("clip-s5");
    checkNear(loadedClip.fadeInSec, 1.25, 0.001);
    checkNear(loadedClip.fadeOutSec, 2.50, 0.001);
    CHECK_EQ(loadedClip.keyframes.size(), 1);
    CHECK_EQ(loadedClip.keyframes[0].seqTime, core::Rational(1));
    checkNear(loadedClip.keyframes[0].transform.scale, 1.5, 0.001);
    checkNear(loadedClip.keyframes[0].transform.x, 25.0, 0.001);
    checkNear(loadedClip.keyframes[0].opacity, 0.8, 0.001);
    CHECK_EQ(loadedClip.keyframes[0].easing, "ease_in_out");
}

int main() {
    return editor::tests::runAll();
}
