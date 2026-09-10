#include "TestHarness.hpp"

#include "core/Model.hpp"

using namespace editor::core;

namespace {
Project makeProject() {
    Project p;
    p.name = "test";
    Asset a;
    a.id = "asset-1";
    a.kind = AssetKind::Video;
    a.path = "clip.mp4";
    a.duration = Rational(8, 1);
    a.fps = Rational(30, 1);
    a.width = 1280;
    a.height = 720;
    p.assets.emplace(a.id, a);

    Sequence seq;
    seq.id = "seq-1";
    seq.fps = Rational(30, 1);
    Track t;
    t.id = "track-1";
    t.kind = TrackKind::Video;
    t.name = "V1";
    seq.tracks.push_back(t);
    Clip c;
    c.id = "clip-1";
    c.assetId = "asset-1";
    c.name = "clip";
    c.sourceIn = Rational(0);
    c.sourceOut = Rational(8, 1);
    c.seqStart = Rational(0);
    seq.clips.emplace(c.id, c);
    seq.tracks.front().clipIds.push_back(c.id);
    p.sequences.push_back(std::move(seq));
    p.activeSequenceId = "seq-1";
    return p;
}
} // namespace

TEST_CASE("project: valid fixture passes validation") {
    const Project p = makeProject();
    CHECK(p.validate().isOk());
}

TEST_CASE("project: clip past asset duration is rejected") {
    Project p = makeProject();
    p.sequences.front().clips.at("clip-1").sourceOut = Rational(10, 1);
    CHECK(p.validate().isErr());
}

TEST_CASE("project: missing asset reference is rejected") {
    Project p = makeProject();
    p.assets.clear();
    CHECK(p.validate().isErr());
}

TEST_CASE("project: inverted source range is rejected") {
    Project p = makeProject();
    auto& clip = p.sequences.front().clips.at("clip-1");
    clip.sourceIn = Rational(5, 1);
    clip.sourceOut = Rational(2, 1);
    CHECK(p.validate().isErr());
}

TEST_CASE("project: negative placement is rejected") {
    Project p = makeProject();
    p.sequences.front().clips.at("clip-1").seqStart = Rational(-1, 1);
    CHECK(p.validate().isErr());
}

TEST_CASE("project: dangling track clip id is rejected") {
    Project p = makeProject();
    p.sequences.front().tracks.front().clipIds.push_back("ghost");
    CHECK(p.validate().isErr());
}

TEST_CASE("clip: source mapping is offset-correct") {
    Project p = makeProject();
    auto& clip = p.sequences.front().clips.at("clip-1");
    clip.sourceIn = Rational(2, 1);
    clip.seqStart = Rational(10, 1);
    // seq t=12 -> source 2 + (12-10) = 4.
    CHECK_EQ(clip.mapToSource(Rational(12, 1)), Rational(4, 1));
    CHECK_EQ(clip.seqEnd(), Rational(10 + 6, 1));
}

int main() {
    return editor::tests::runAll();
}
