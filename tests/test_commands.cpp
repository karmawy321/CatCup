#include "TestHarness.hpp"

#include "commands/ClipCommands.hpp"
#include "commands/Command.hpp"
#include "core/Model.hpp"

using namespace editor;

namespace {
core::Project makeProject() {
    core::Project p;
    p.name = "cmds";
    core::Asset a;
    a.id = "asset-1";
    a.duration = core::Rational(8, 1);
    a.fps = core::Rational(30, 1);
    p.assets.emplace(a.id, a);
    core::Sequence seq;
    seq.id = "seq-1";
    core::Track t;
    t.id = "track-1";
    t.kind = core::TrackKind::Video;
    t.name = "V1";
    seq.tracks.push_back(t);
    p.sequences.push_back(seq);
    p.activeSequenceId = "seq-1";
    return p;
}

core::Clip makeClip(const std::string& id) {
    core::Clip c;
    c.id = id;
    c.assetId = "asset-1";
    c.name = "clip";
    c.sourceIn = core::Rational(0);
    c.sourceOut = core::Rational(8, 1);
    c.seqStart = core::Rational(0);
    return c;
}
} // namespace

TEST_CASE("commands: add + undo + redo") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));
    CHECK_EQ(p.sequences.front().clips.count("c1"), 1);
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.count("c1"), 0);
    CHECK(undo.redo(p, error));
    CHECK_EQ(p.sequences.front().clips.count("c1"), 1);
}

TEST_CASE("commands: locked track rejects edits") {
    core::Project p = makeProject();
    p.sequences.front().tracks.front().locked = true;
    commands::UndoStack undo(10);
    std::string error;
    CHECK(!undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));
    CHECK(!error.empty());
    CHECK_EQ(p.sequences.front().clips.size(), 0);
}

TEST_CASE("commands: split produces two frame-exact halves") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));
    CHECK(undo.execute(commands::makeSplitClipCommand("c1", core::Rational(3, 1), "c2"), p, error));
    const auto& left = p.sequences.front().clips.at("c1");
    const auto& right = p.sequences.front().clips.at("c2");
    CHECK_EQ(left.sourceOut, core::Rational(3, 1));
    CHECK_EQ(right.sourceIn, core::Rational(3, 1));
    CHECK_EQ(right.sourceOut, core::Rational(8, 1));
    CHECK_EQ(right.seqStart, core::Rational(3, 1));
    // Total duration preserved: 3 s + 5 s == 8 s.
    CHECK_EQ(left.seqDuration() + right.seqDuration(), core::Rational(8, 1));
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.count("c2"), 0);
    CHECK_EQ(p.sequences.front().clips.at("c1").sourceOut, core::Rational(8, 1));
}

TEST_CASE("commands: split outside clip is rejected") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));
    CHECK(!undo.execute(commands::makeSplitClipCommand("c1", core::Rational(20, 1), "c2"), p, error));
    CHECK_EQ(p.sequences.front().clips.count("c2"), 0);
}

TEST_CASE("commands: trim + move round-trip through undo") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));
    CHECK(undo.execute(commands::makeTrimClipCommand("c1", core::Rational(1, 1),
                                                     core::Rational(6, 1), core::Rational(2, 1)),
                       p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").seqStart, core::Rational(2, 1));
    CHECK(undo.execute(commands::makeMoveClipCommand("c1", core::Rational(4, 1)), p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").seqStart, core::Rational(4, 1));
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("c1").seqStart, core::Rational(2, 1));
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("c1").sourceIn, core::Rational(0));
}

TEST_CASE("commands: redo is cleared by a new edit") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));
    CHECK(undo.undo(p));
    CHECK(undo.canRedo());
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c9")), p, error));
    CHECK(!undo.canRedo());
}

TEST_CASE("commands: remove restores position on undo") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c2")), p, error));
    CHECK(undo.execute(commands::makeRemoveClipCommand("track-1", "c1"), p, error));
    CHECK_EQ(p.sequences.front().tracks.front().clipIds.size(), 1);
    CHECK(undo.undo(p));
    const auto& ids = p.sequences.front().tracks.front().clipIds;
    CHECK_EQ(ids.size(), 2);
    CHECK_EQ(ids[0], std::string("c1")); // original z-order restored
}

TEST_CASE("commands: set transform is validated and undoable") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));
    core::Transform tr;
    tr.scale = 2.0;
    tr.x = 100.0;
    tr.y = -50.0;
    tr.rotationDeg = 15.0;
    CHECK(undo.execute(commands::makeSetTransformCommand("c1", tr), p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").transform.scale, 2.0);
    tr.scale = 0.0; // invalid: rejected, previous value kept
    CHECK(!undo.execute(commands::makeSetTransformCommand("c1", tr), p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").transform.scale, 2.0);
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("c1").transform.scale, 1.0);
}

TEST_CASE("commands: set text is undoable") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    core::Clip title = makeClip("t1");
    title.assetId.clear();
    title.text = "before";
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", title), p, error));
    CHECK(undo.execute(commands::makeSetTextCommand("t1", "after", "Arial", 72.0), p, error));
    CHECK_EQ(p.sequences.front().clips.at("t1").text, std::string("after"));
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("t1").text, std::string("before"));
}

int main() {
    return editor::tests::runAll();
}
