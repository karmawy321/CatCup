#include "TestHarness.hpp"

#include "commands/ClipCommands.hpp"
#include "commands/Command.hpp"
#include "commands/EffectCommands.hpp"
#include "commands/TransitionCommands.hpp"
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

TEST_CASE("commands: set opacity is undoable and clamped") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));
    CHECK(undo.execute(commands::makeSetOpacityCommand("c1", 0.35), p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").opacity, 0.35);
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("c1").opacity, 1.0);
    CHECK(undo.redo(p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").opacity, 0.35);
}

TEST_CASE("commands: ripple delete closes timeline gap") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    core::Clip c1 = makeClip("c1");
    c1.seqStart = core::Rational(0);
    c1.sourceOut = core::Rational(3, 1);
    core::Clip c2 = makeClip("c2");
    c2.seqStart = core::Rational(3, 1);
    c2.sourceOut = core::Rational(5, 1);
    core::Clip c3 = makeClip("c3");
    c3.seqStart = core::Rational(8, 1);
    c3.sourceOut = core::Rational(4, 1);

    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c1), p, error));
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c2), p, error));
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c3), p, error));

    // Ripple delete c2 (duration 5s). c3 was at 8s, should shift left by 5s to 3s.
    CHECK(undo.execute(commands::makeRippleDeleteClipCommand("track-1", "c2"), p, error));
    CHECK_EQ(p.sequences.front().clips.count("c2"), 0);
    CHECK_EQ(p.sequences.front().clips.at("c3").seqStart, core::Rational(3, 1));

    // Undo restores c2 and c3's position at 8s
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.count("c2"), 1);
    CHECK_EQ(p.sequences.front().clips.at("c2").seqStart, core::Rational(3, 1));
    CHECK_EQ(p.sequences.front().clips.at("c3").seqStart, core::Rational(8, 1));
}

TEST_CASE("commands: ripple trim shifts following clips by delta") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    core::Clip c1 = makeClip("c1");
    c1.seqStart = core::Rational(0);
    c1.sourceOut = core::Rational(4, 1); // duration 4s
    core::Clip c2 = makeClip("c2");
    c2.seqStart = core::Rational(4, 1);
    c2.sourceOut = core::Rational(4, 1);

    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c1), p, error));
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c2), p, error));

    // Shorten c1 from 4s to 2s via ripple trim. c2 should shift from 4s to 2s.
    CHECK(undo.execute(commands::makeRippleTrimClipCommand("c1", core::Rational(0), core::Rational(2, 1), core::Rational(0)), p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").seqDuration(), core::Rational(2, 1));
    CHECK_EQ(p.sequences.front().clips.at("c2").seqStart, core::Rational(2, 1));

    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("c1").seqDuration(), core::Rational(4, 1));
    CHECK_EQ(p.sequences.front().clips.at("c2").seqStart, core::Rational(4, 1));
}

TEST_CASE("commands: transition add, update, remove, and cascade delete") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    core::Clip c1 = makeClip("c1");
    c1.seqStart = core::Rational(0);
    c1.sourceOut = core::Rational(4, 1);
    core::Clip c2 = makeClip("c2");
    c2.seqStart = core::Rational(4, 1);
    c2.sourceOut = core::Rational(4, 1);

    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c1), p, error));
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c2), p, error));

    core::Transition tr;
    tr.id = "tr1";
    tr.trackId = "track-1";
    tr.fromClipId = "c1";
    tr.toClipId = "c2";
    tr.type = "crossfade";
    tr.duration = core::Rational(1, 1);
    tr.alignment = core::TransitionAlignment::CenterOnCut;

    // Add transition
    CHECK(undo.execute(commands::makeAddTransitionCommand(tr), p, error));
    CHECK_EQ(p.sequences.front().transitions.size(), 1);

    // Update transition
    CHECK(undo.execute(commands::makeUpdateTransitionCommand("tr1", core::Rational(2, 1), core::TransitionAlignment::StartOnCut, "dip_black", "linear"), p, error));
    CHECK_EQ(p.sequences.front().transitions.front().duration, core::Rational(2, 1));
    CHECK_EQ(p.sequences.front().transitions.front().type, std::string("dip_black"));

    // Remove transition
    CHECK(undo.execute(commands::makeRemoveTransitionCommand("tr1"), p, error));
    CHECK_EQ(p.sequences.front().transitions.size(), 0);

    // Undo remove
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().transitions.size(), 1);

    // Removing a clip that has an attached transition cleans up the transition
    CHECK(undo.execute(commands::makeRemoveClipCommand("track-1", "c1"), p, error));
    CHECK_EQ(p.sequences.front().transitions.size(), 0);

    // Undo restoring the clip also restores its transition
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().transitions.size(), 1);
}

TEST_CASE("commands: effect add + update + remove + undo") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", makeClip("c1")), p, error));

    core::Effect eff;
    eff.type = "vignette";
    eff.enabled = true;
    eff.order = 0;
    eff.params["intensity"] = 0.8;
    eff.params["radius"] = 0.5;

    // Add effect
    CHECK(undo.execute(commands::makeAddEffectCommand("c1", eff), p, error));
    const auto& clip = p.sequences.front().clips.at("c1");
    CHECK_EQ(clip.effects.size(), 1);
    CHECK_EQ(clip.effects[0].type, std::string("vignette"));
    CHECK_EQ(clip.effects[0].params.at("intensity"), 0.8);

    // Update effect param
    CHECK(undo.execute(commands::makeUpdateEffectCommand("c1", 0, {{"intensity", 0.4}, {"radius", 0.5}}, {}), p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").effects[0].params.at("intensity"), 0.4);

    // Undo update
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("c1").effects[0].params.at("intensity"), 0.8);

    // Redo update
    CHECK(undo.redo(p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").effects[0].params.at("intensity"), 0.4);

    // Remove effect
    CHECK(undo.execute(commands::makeRemoveEffectCommand("c1", 0), p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").effects.size(), 0);

    // Undo remove
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("c1").effects.size(), 1);
    CHECK_EQ(p.sequences.front().clips.at("c1").effects[0].type, std::string("vignette"));
}

TEST_CASE("commands: set clip speed with and without ripple") {
    core::Project p = makeProject();
    commands::UndoStack undo(10);
    std::string error;

    core::Clip c1 = makeClip("c1");
    c1.seqStart = core::Rational(0);
    c1.sourceOut = core::Rational(4, 1); // seqDuration = 4s

    core::Clip c2 = makeClip("c2");
    c2.seqStart = core::Rational(4, 1);
    c2.sourceOut = core::Rational(4, 1); // seqDuration = 4s

    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c1), p, error));
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c2), p, error));

    // 1. Set speed without ripple: 2.0x (Rational(2, 1))
    // c1 duration becomes 4 / 2 = 2s. c2 start remains 4s (gap of 2s).
    CHECK(undo.execute(commands::makeSetClipSpeedCommand("c1", core::Rational(2, 1), false), p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").speed, core::Rational(2, 1));
    CHECK_EQ(p.sequences.front().clips.at("c1").seqDuration(), core::Rational(2, 1));
    CHECK_EQ(p.sequences.front().clips.at("c2").seqStart, core::Rational(4, 1));

    // Undo: c1 speed restored to 1.0x
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("c1").speed, core::Rational(1, 1));
    CHECK_EQ(p.sequences.front().clips.at("c1").seqDuration(), core::Rational(4, 1));

    // 2. Set speed WITH ripple: 2.0x
    // c1 duration becomes 2s (delta = -2s). c2 start should shift from 4s to 2s.
    CHECK(undo.execute(commands::makeSetClipSpeedCommand("c1", core::Rational(2, 1), true), p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").speed, core::Rational(2, 1));
    CHECK_EQ(p.sequences.front().clips.at("c1").seqDuration(), core::Rational(2, 1));
    CHECK_EQ(p.sequences.front().clips.at("c2").seqStart, core::Rational(2, 1));

    // Undo ripple: c1 duration is 4s, c2 start is 4s
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().clips.at("c1").speed, core::Rational(1, 1));
    CHECK_EQ(p.sequences.front().clips.at("c2").seqStart, core::Rational(4, 1));

    // Redo ripple: c1 duration is 2s, c2 start is 2s
    CHECK(undo.redo(p, error));
    CHECK_EQ(p.sequences.front().clips.at("c1").speed, core::Rational(2, 1));
    CHECK_EQ(p.sequences.front().clips.at("c2").seqStart, core::Rational(2, 1));
}

TEST_CASE("commands: move clip to track with undo/redo") {
    core::Project p = makeProject();
    core::Track t2;
    t2.id = "track-2";
    t2.kind = core::TrackKind::Video;
    t2.name = "V2";
    p.sequences.front().tracks.push_back(t2);

    commands::UndoStack undo(10);
    std::string error;
    core::Clip c = makeClip("c1");
    CHECK(undo.execute(commands::makeAddClipCommand("track-1", c), p, error));
    CHECK_EQ(p.sequences.front().tracks[0].clipIds.size(), 1u);
    CHECK_EQ(p.sequences.front().tracks[1].clipIds.size(), 0u);

    // Move to track-2 at 3.5s
    CHECK(undo.execute(commands::makeMoveClipToTrackCommand("c1", "track-2", core::Rational(7, 2)), p, error));
    CHECK_EQ(p.sequences.front().tracks[0].clipIds.size(), 0u);
    CHECK_EQ(p.sequences.front().tracks[1].clipIds.size(), 1u);
    CHECK_EQ(p.sequences.front().clips.at("c1").seqStart, core::Rational(7, 2));

    // Undo: back to track-1 at 0s
    CHECK(undo.undo(p));
    CHECK_EQ(p.sequences.front().tracks[0].clipIds.size(), 1u);
    CHECK_EQ(p.sequences.front().tracks[1].clipIds.size(), 0u);
    CHECK_EQ(p.sequences.front().clips.at("c1").seqStart, core::Rational(0));

    // Redo: to track-2 at 3.5s
    CHECK(undo.redo(p, error));
    CHECK_EQ(p.sequences.front().tracks[0].clipIds.size(), 0u);
    CHECK_EQ(p.sequences.front().tracks[1].clipIds.size(), 1u);
    CHECK_EQ(p.sequences.front().clips.at("c1").seqStart, core::Rational(7, 2));
}

int main() {
    return editor::tests::runAll();
}

