#include "TestHarness.hpp"

#include "ai/AudioAnalysis.hpp"
#include "ai/CaptionEngine.hpp"
#include "ai/SceneDetection.hpp"
#include "commands/ClipCommands.hpp"
#include "commands/SmartCommands.hpp"
#include "commands/Command.hpp"
#include "core/Model.hpp"

#include <cmath>
#include <vector>

using namespace editor;

TEST_CASE("smart: audio rms and silence detection") {
    const int sampleRate = 48000;
    const int channels = 1;
    // 3 seconds total:
    // [0s .. 1s]: Loud 440 Hz sine wave (amp 0.8)
    // [1s .. 2s]: Total silence (amp 0.0)
    // [2s .. 3s]: Loud 440 Hz sine wave (amp 0.8)
    std::vector<float> pcm(sampleRate * 3, 0.0f);
    for (int i = 0; i < sampleRate; ++i) {
        pcm[static_cast<std::size_t>(i)] = 0.8f * std::sin(2.0f * 3.14159265f * 440.0f * (i / 48000.0f));
    }
    for (int i = sampleRate * 2; i < sampleRate * 3; ++i) {
        pcm[static_cast<std::size_t>(i)] = 0.8f * std::sin(2.0f * 3.14159265f * 440.0f * (i / 48000.0f));
    }

    // RMS of silence part
    double silRms = ai::AudioAnalysis::calculateRmsDb(pcm.data() + sampleRate, sampleRate);
    CHECK(silRms < -60.0);

    // RMS of loud part
    double loudRms = ai::AudioAnalysis::calculateRmsDb(pcm.data(), sampleRate);
    CHECK(loudRms > -10.0);

    // Detect silences
    auto silences = ai::AudioAnalysis::detectSilences(
        pcm.data(), pcm.size(), sampleRate, channels, -35.0, 0.4
    );

    CHECK_EQ(silences.size(), 1);
    const double silStart = static_cast<double>(silences[0].startSec);
    const double silDur = static_cast<double>(silences[0].durationSec);
    CHECK(silStart >= 0.95 && silStart <= 1.05);
    CHECK(silDur >= 0.95 && silDur <= 1.05);
}

TEST_CASE("smart: video frame difference and scene cut detection") {
    const int w = 4;
    const int h = 4;
    const int stride = w * 4;
    std::vector<uint8_t> redFrame(w * h * 4, 0);
    std::vector<uint8_t> blueFrame(w * h * 4, 0);

    for (int i = 0; i < w * h; ++i) {
        redFrame[static_cast<std::size_t>(i * 4 + 0)] = 255;
        redFrame[static_cast<std::size_t>(i * 4 + 3)] = 255;

        blueFrame[static_cast<std::size_t>(i * 4 + 2)] = 255;
        blueFrame[static_cast<std::size_t>(i * 4 + 3)] = 255;
    }

    // Same frame difference is 0
    double diffSame = ai::SceneDetection::calculateFrameDifference(
        redFrame.data(), redFrame.data(), w, h, stride
    );
    CHECK_EQ(diffSame, 0.0);

    // Red vs Blue difference: (255 red + 255 blue) / (3 * 255) = 2/3 ~= 0.667
    double diffOpposite = ai::SceneDetection::calculateFrameDifference(
        redFrame.data(), blueFrame.data(), w, h, stride
    );
    CHECK(diffOpposite > 0.65 && diffOpposite < 0.68);

    // Scene cut peak detection
    std::vector<core::Rational> times = {
        core::Rational(0), core::Rational(1, 1), core::Rational(2, 1),
        core::Rational(3, 1), core::Rational(4, 1)
    };
    std::vector<double> scores = {0.02, 0.03, 0.75, 0.04, 0.02};

    auto cuts = ai::SceneDetection::findCutsFromScores(times, scores, 0.25);
    CHECK_EQ(cuts.size(), 1);
    CHECK_EQ(cuts[0].timestamp, core::Rational(2, 1));
}

TEST_CASE("smart: caption SRT parse and export round-trip") {
    const std::string sampleSrt =
        "1\n"
        "00:00:01,500 --> 00:00:03,000\n"
        "Welcome to CatCup editor\n"
        "\n"
        "2\n"
        "00:00:03,500 --> 00:00:06,250\n"
        "Modern fast and native\n\n";

    auto cues = ai::CaptionEngine::parseSrt(sampleSrt);
    CHECK_EQ(cues.size(), 2);
    CHECK_EQ(cues[0].startSec, core::Rational(15, 10));
    CHECK_EQ(cues[0].durationSec, core::Rational(15, 10)); // 3.0 - 1.5 = 1.5s
    CHECK_EQ(cues[0].text, std::string("Welcome to CatCup editor"));

    CHECK_EQ(cues[1].startSec, core::Rational(35, 10));
    CHECK_EQ(cues[1].durationSec, core::Rational(275, 100)); // 6.25 - 3.5 = 2.75s
    CHECK_EQ(cues[1].text, std::string("Modern fast and native"));

    // Export to SRT
    std::string exported = ai::CaptionEngine::exportSrt(cues);
    CHECK(!exported.empty());

    // Re-parse exported
    auto reParsed = ai::CaptionEngine::parseSrt(exported);
    CHECK_EQ(reParsed.size(), 2);
    CHECK_EQ(reParsed[0].text, cues[0].text);
    CHECK_EQ(reParsed[1].text, cues[1].text);
}

TEST_CASE("smart: transcript chunking into timed cues") {
    const std::string text = "CatCup native desktop editor built with modern C cpp";
    // 9 words, 3 words per cue -> 3 cues
    auto cues = ai::CaptionEngine::chunkTranscript(text, core::Rational(9, 1), 3);
    CHECK_EQ(cues.size(), 3);
    CHECK_EQ(cues[0].text, std::string("CatCup native desktop"));
    CHECK_EQ(cues[0].startSec, core::Rational(0));
    CHECK_EQ(cues[0].durationSec, core::Rational(3, 1));

    CHECK_EQ(cues[1].text, std::string("editor built with"));
    CHECK_EQ(cues[1].startSec, core::Rational(3, 1));
    CHECK_EQ(cues[1].durationSec, core::Rational(3, 1));

    CHECK_EQ(cues[2].text, std::string("modern C cpp"));
    CHECK_EQ(cues[2].startSec, core::Rational(6, 1));
    CHECK_EQ(cues[2].durationSec, core::Rational(3, 1));
}

TEST_CASE("smart: silence cut command with ripple") {
    core::Project p;
    p.name = "smart_silence_test";
    core::Sequence seq;
    seq.id = "s1";
    core::Track t;
    t.id = "v1";
    t.kind = core::TrackKind::Video;
    seq.tracks.push_back(t);

    core::Clip c1;
    c1.id = "c1";
    c1.name = "clip1";
    c1.sourceIn = core::Rational(0);
    c1.sourceOut = core::Rational(6, 1);
    c1.seqStart = core::Rational(0); // 6s duration

    seq.clips.emplace(c1.id, c1);
    seq.tracks.front().clipIds.push_back(c1.id);
    p.sequences.push_back(seq);
    p.activeSequenceId = "s1";

    commands::UndoStack undo(10);
    std::string err;

    // Silence from 2s to 4s (duration 2s)
    std::vector<ai::SilenceInterval> silences = {
        {core::Rational(2, 1), core::Rational(2, 1)}
    };

    // Cut silence
    CHECK(undo.execute(commands::makeSilenceCutCommand("v1", "c1", silences), p, err));

    // Original clip removed, replaced by two ripple-joined subclips:
    // Subclip 1: [0..2s] (duration 2s)
    // Subclip 2: [2..4s] (duration 2s, source [4..6s])
    const auto& currentClips = p.sequences.front().tracks.front().clipIds;
    CHECK_EQ(currentClips.size(), 2);

    const auto& piece1 = p.sequences.front().clips.at(currentClips[0]);
    const auto& piece2 = p.sequences.front().clips.at(currentClips[1]);
    CHECK_EQ(piece1.seqStart, core::Rational(0));
    CHECK_EQ(piece1.seqDuration(), core::Rational(2, 1));

    CHECK_EQ(piece2.seqStart, core::Rational(2, 1));
    CHECK_EQ(piece2.seqDuration(), core::Rational(2, 1));
    CHECK_EQ(piece2.sourceIn, core::Rational(4, 1));

    // Undo: original clip restored
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().tracks.front().clipIds.size(), 1);
    CHECK_EQ(p.sequences.front().clips.at("c1").seqDuration(), core::Rational(6, 1));
}

TEST_CASE("smart: scene split command") {
    core::Project p;
    p.name = "smart_scene_test";
    core::Sequence seq;
    seq.id = "s1";
    core::Track t;
    t.id = "v1";
    t.kind = core::TrackKind::Video;
    seq.tracks.push_back(t);

    core::Clip c1;
    c1.id = "c1";
    c1.name = "clip1";
    c1.sourceIn = core::Rational(0);
    c1.sourceOut = core::Rational(8, 1);
    c1.seqStart = core::Rational(0);

    seq.clips.emplace(c1.id, c1);
    seq.tracks.front().clipIds.push_back(c1.id);
    p.sequences.push_back(seq);
    p.activeSequenceId = "s1";

    commands::UndoStack undo(10);
    std::string err;

    // Splits at 3s and 5s
    std::vector<core::Rational> cutPoints = {core::Rational(3, 1), core::Rational(5, 1)};

    CHECK(undo.execute(commands::makeSceneSplitCommand("c1", cutPoints), p, err));
    const auto& currentClips = p.sequences.front().tracks.front().clipIds;
    CHECK_EQ(currentClips.size(), 3);

    const auto& p1 = p.sequences.front().clips.at(currentClips[0]);
    const auto& p2 = p.sequences.front().clips.at(currentClips[1]);
    const auto& p3 = p.sequences.front().clips.at(currentClips[2]);

    CHECK_EQ(p1.seqStart, core::Rational(0));
    CHECK_EQ(p1.seqDuration(), core::Rational(3, 1));

    CHECK_EQ(p2.seqStart, core::Rational(3, 1));
    CHECK_EQ(p2.seqDuration(), core::Rational(2, 1));

    CHECK_EQ(p3.seqStart, core::Rational(5, 1));
    CHECK_EQ(p3.seqDuration(), core::Rational(3, 1));

    // Undo restores original clip
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().tracks.front().clipIds.size(), 1);
    CHECK_EQ(p.sequences.front().clips.at("c1").seqDuration(), core::Rational(8, 1));
}

TEST_CASE("smart: add captions command") {
    core::Project p;
    p.name = "smart_captions_test";
    core::Sequence seq;
    seq.id = "s1";
    core::Track t;
    t.id = "t1";
    t.kind = core::TrackKind::Text;
    seq.tracks.push_back(t);
    p.sequences.push_back(seq);
    p.activeSequenceId = "s1";

    commands::UndoStack undo(10);
    std::string err;

    std::vector<ai::CaptionCue> cues = {
        {core::Rational(1, 1), core::Rational(2, 1), "Subtitle 1"},
        {core::Rational(3, 1), core::Rational(2, 1), "Subtitle 2"}
    };

    CHECK(undo.execute(commands::makeAddCaptionsCommand("t1", cues), p, err));
    CHECK_EQ(p.sequences.front().tracks.front().clipIds.size(), 2);
    CHECK_EQ(p.sequences.front().clips.size(), 2);

    // Undo
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().tracks.front().clipIds.size(), 0);
    CHECK_EQ(p.sequences.front().clips.size(), 0);
}

int main() {
    return editor::tests::runAll();
}
