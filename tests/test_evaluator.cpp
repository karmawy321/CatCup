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

int main() {
    return editor::tests::runAll();
}
