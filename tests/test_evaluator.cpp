#include "TestHarness.hpp"

#include "core/Model.hpp"
#include "render/Evaluator.hpp"

using namespace editor;

TEST_CASE("evaluator: gap yields no layers, clip yields one") {
    core::Sequence seq;
    seq.id = "s";
    core::Track t;
    t.id = "v";
    t.kind = core::TrackKind::Video;
    seq.tracks.push_back(t);
    core::Clip c;
    c.id = "c";
    c.sourceIn = core::Rational(0);
    c.sourceOut = core::Rational(4, 1);
    c.seqStart = core::Rational(2, 1); // occupies [2,6)
    seq.clips.emplace(c.id, c);
    seq.tracks.front().clipIds.push_back(c.id);

    CHECK(render::Evaluator::evaluateVideoAt(seq, core::Rational(0)).layers.empty());
    const auto plan = render::Evaluator::evaluateVideoAt(seq, core::Rational(3, 1));
    CHECK_EQ(plan.layers.size(), 1);
    // seq 3 -> source 0 + (3-2) = 1.
    CHECK_EQ(plan.layers[0].sourceTime, core::Rational(1, 1));
    CHECK(render::Evaluator::evaluateVideoAt(seq, core::Rational(6, 1)).layers.empty());
}

TEST_CASE("evaluator: overlapping tracks stack back-to-front") {
    core::Sequence seq;
    seq.id = "s";
    for (int i = 0; i < 2; ++i) {
        core::Track t;
        t.id = "v" + std::to_string(i);
        t.kind = core::TrackKind::Video;
        seq.tracks.push_back(t);
    }
    for (int i = 0; i < 2; ++i) {
        core::Clip c;
        c.id = "c" + std::to_string(i);
        c.sourceIn = core::Rational(0);
        c.sourceOut = core::Rational(4, 1);
        c.seqStart = core::Rational(0);
        seq.clips.emplace(c.id, c);
        seq.tracks[static_cast<std::size_t>(i)].clipIds.push_back(c.id);
    }
    const auto plan = render::Evaluator::evaluateVideoAt(seq, core::Rational(1, 1));
    CHECK_EQ(plan.layers.size(), 2);
    CHECK(plan.layers[0].zOrder < plan.layers[1].zOrder);
}

TEST_CASE("evaluator: muted audio and hidden video are skipped") {
    core::Sequence seq;
    seq.id = "s";
    core::Track v;
    v.id = "v";
    v.kind = core::TrackKind::Video;
    v.visible = false;
    core::Track a;
    a.id = "a";
    a.kind = core::TrackKind::Audio;
    a.muted = true;
    seq.tracks.push_back(v);
    seq.tracks.push_back(a);
    core::Clip cv;
    cv.id = "cv";
    cv.sourceIn = core::Rational(0);
    cv.sourceOut = core::Rational(4, 1);
    cv.seqStart = core::Rational(0);
    core::Clip ca = cv;
    ca.id = "ca";
    seq.clips.emplace(cv.id, cv);
    seq.clips.emplace(ca.id, ca);
    seq.tracks[0].clipIds.push_back(cv.id);
    seq.tracks[1].clipIds.push_back(ca.id);

    CHECK(render::Evaluator::evaluateVideoAt(seq, core::Rational(1, 1)).layers.empty());
    CHECK(render::Evaluator::evaluateAudioAt(seq, core::Rational(1, 1)).empty());
}

TEST_CASE("evaluator: sequence duration is the max clip end") {
    core::Sequence seq;
    seq.id = "s";
    core::Track t;
    t.id = "v";
    t.kind = core::TrackKind::Video;
    seq.tracks.push_back(t);
    for (int i = 0; i < 2; ++i) {
        core::Clip c;
        c.id = "c" + std::to_string(i);
        c.sourceIn = core::Rational(0);
        c.sourceOut = core::Rational(2, 1);
        c.seqStart = core::Rational(i == 0 ? 0 : 5, 1);
        seq.clips.emplace(c.id, c);
    }
    CHECK_EQ(render::Evaluator::sequenceDuration(seq), core::Rational(7, 1));
}

TEST_CASE("evaluator: clip opacity is propagated to layer") {
    core::Sequence seq;
    seq.id = "s";
    core::Track t;
    t.id = "v";
    t.kind = core::TrackKind::Video;
    seq.tracks.push_back(t);
    core::Clip c;
    c.id = "c";
    c.sourceIn = core::Rational(0);
    c.sourceOut = core::Rational(4, 1);
    c.seqStart = core::Rational(0);
    c.opacity = 0.4;
    seq.clips.emplace(c.id, c);
    seq.tracks.front().clipIds.push_back(c.id);

    const auto plan = render::Evaluator::evaluateVideoAt(seq, core::Rational(1, 1));
    CHECK_EQ(plan.layers.size(), 1);
    CHECK_EQ(plan.layers[0].opacity, 0.4);
}

TEST_CASE("evaluator: transition blends incoming and outgoing clips") {
    core::Sequence seq;
    seq.id = "s";
    core::Track t;
    t.id = "v";
    t.kind = core::TrackKind::Video;
    seq.tracks.push_back(t);

    core::Clip c1;
    c1.id = "c1";
    c1.assetId = "a1";
    c1.sourceIn = core::Rational(0);
    c1.sourceOut = core::Rational(4, 1);
    c1.seqStart = core::Rational(0);

    core::Clip c2;
    c2.id = "c2";
    c2.assetId = "a2";
    c2.sourceIn = core::Rational(0);
    c2.sourceOut = core::Rational(4, 1);
    c2.seqStart = core::Rational(4, 1);

    seq.clips.emplace(c1.id, c1);
    seq.clips.emplace(c2.id, c2);
    seq.tracks.front().clipIds.push_back(c1.id);
    seq.tracks.front().clipIds.push_back(c2.id);

    // Transition of 2 seconds, centered on cut (cut is at 4s, so transition runs [3, 5))
    core::Transition tr;
    tr.id = "tr1";
    tr.trackId = "v";
    tr.fromClipId = "c1";
    tr.toClipId = "c2";
    tr.type = "crossfade";
    tr.duration = core::Rational(2, 1);
    tr.alignment = core::TransitionAlignment::CenterOnCut;
    seq.transitions.push_back(tr);

    // Before transition window (at 2s): normal c1 clip
    auto planBefore = render::Evaluator::evaluateVideoAt(seq, core::Rational(2, 1));
    CHECK_EQ(planBefore.layers.size(), 1);
    CHECK(!planBefore.layers[0].inTransition);

    // In middle of transition window (at 4s - exact cut):
    // progress is (4 - 3) / 2 = 0.5
    auto planMid = render::Evaluator::evaluateVideoAt(seq, core::Rational(4, 1));
    CHECK_EQ(planMid.layers.size(), 1);
    CHECK(planMid.layers[0].inTransition);
    CHECK_EQ(planMid.layers[0].transitionType, std::string("crossfade"));
    CHECK_EQ(planMid.layers[0].assetId, std::string("a1"));
    CHECK_EQ(planMid.layers[0].secondaryAssetId, std::string("a2"));
    CHECK_EQ(planMid.layers[0].blendFactor, 0.5);

    // Audio evaluation on an audio track
    core::Track a;
    a.id = "a";
    a.kind = core::TrackKind::Audio;
    seq.tracks.push_back(a);
    core::Clip ca1 = c1;
    ca1.id = "ca1";
    core::Clip ca2 = c2;
    ca2.id = "ca2";
    seq.clips.emplace(ca1.id, ca1);
    seq.clips.emplace(ca2.id, ca2);
    seq.tracks[1].clipIds.push_back(ca1.id);
    seq.tracks[1].clipIds.push_back(ca2.id);

    core::Transition tra = tr;
    tra.id = "tra1";
    tra.trackId = "a";
    tra.fromClipId = "ca1";
    tra.toClipId = "ca2";
    seq.transitions.push_back(tra);

    // Audio evaluation at 4s: both audio clips contribute overlapping spans
    auto audioSpans = render::Evaluator::evaluateAudioAt(seq, core::Rational(4, 1));
    CHECK_EQ(audioSpans.size(), 2);
    CHECK_EQ(audioSpans[0].clipId, std::string("ca1"));
    CHECK_EQ(audioSpans[1].clipId, std::string("ca2"));
}

int main() {
    return editor::tests::runAll();
}
