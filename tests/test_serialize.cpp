#include "TestHarness.hpp"

#include "core/Model.hpp"
#include "persist/AtomicFile.hpp"
#include "persist/Json.hpp"
#include "persist/Migrations.hpp"
#include "persist/ProjectSerializer.hpp"

#include <filesystem>

using namespace editor;

TEST_CASE("json: round-trips tricky strings (unicode, quotes, emoji)") {
    persist::JsonValue v(persist::JsonObject{
        {"text", persist::JsonValue(std::string("Grüße \"Café\" 🎬 裁剪"))},
        {"n", persist::JsonValue(std::int64_t{42})},
    });
    const std::string dumped = persist::dumpJson(v, false);
    auto back = persist::parseJson(dumped);
    CHECK(back.isOk());
    const auto* found = back.value().find("text");
    CHECK(found != nullptr && found->isString());
    CHECK_EQ(found->asString(), std::string("Grüße \"Café\" 🎬 裁剪"));
}

TEST_CASE("serialize: rational stays exact (no double drift)") {
    core::Project p;
    p.name = "exact";
    core::Asset a;
    a.id = "a1";
    a.duration = core::Rational(1, 3); // repeating decimal — must not drift
    a.fps = core::Rational(30000, 1001);
    p.assets.emplace(a.id, a);
    core::Sequence seq;
    seq.id = "s1";
    seq.fps = core::Rational(30000, 1001);
    core::Track t;
    t.id = "t1";
    seq.tracks.push_back(t);
    p.sequences.push_back(seq);
    p.activeSequenceId = "s1";

    auto json = persist::projectToJson(p);
    CHECK(json.isOk());
    auto back = persist::projectFromJson(json.value());
    CHECK(back.isOk());
    CHECK_EQ(back.value().assets.at("a1").duration, core::Rational(1, 3));
    CHECK_EQ(back.value().assets.at("a1").fps, core::Rational(30000, 1001));
}

TEST_CASE("serialize: full project round-trip") {
    core::Project p;
    p.name = "0907";
    core::Asset a;
    a.id = "asset-1";
    a.kind = core::AssetKind::Video;
    a.path = "research/fixtures/inspection-test.mp4";
    a.duration = core::Rational(8, 1);
    a.fps = core::Rational(30, 1);
    a.width = 1280;
    a.height = 720;
    p.assets.emplace(a.id, a);

    core::Sequence seq;
    seq.id = "seq-1";
    core::Track video;
    video.id = "v1";
    video.kind = core::TrackKind::Video;
    video.name = "V1";
    core::Track text;
    text.id = "t1";
    text.kind = core::TrackKind::Text;
    text.name = "T1";
    seq.tracks.push_back(video);
    seq.tracks.push_back(text);
    core::Clip c;
    c.id = "c1";
    c.assetId = "asset-1";
    c.name = "inspection-test";
    c.sourceIn = core::Rational(0);
    c.sourceOut = core::Rational(8, 1);
    c.seqStart = core::Rational(0);
    seq.clips.emplace(c.id, c);
    seq.tracks[0].clipIds.push_back(c.id);
    core::Clip title;
    title.id = "c2";
    title.name = "Title";
    title.sourceIn = core::Rational(0);
    title.sourceOut = core::Rational(3, 1);
    title.seqStart = core::Rational(1, 1);
    title.text = "Hello";
    seq.clips.emplace(title.id, title);
    seq.tracks[1].clipIds.push_back(title.id);
    p.sequences.push_back(seq);
    p.activeSequenceId = "seq-1";

    auto json = persist::projectToJson(p);
    CHECK(json.isOk());
    // Must be human-inspectable (BUILD-PLAN requirement).
    const std::string text1 = persist::dumpJson(json.value(), true);
    CHECK(text1.find("inspection-test.mp4") != std::string::npos);
    auto back = persist::projectFromJson(persist::parseJson(text1).value());
    CHECK(back.isOk());
    CHECK_EQ(back.value().sequences.size(), 1);
    CHECK_EQ(back.value().sequences.front().clips.size(), 2);
    CHECK_EQ(back.value().sequences.front().clips.at("c2").text, std::string("Hello"));
}

TEST_CASE("migrations: future schema is rejected, not silently loaded") {
    persist::JsonValue doc(persist::JsonObject{
        {"schemaVersion", persist::JsonValue(std::int64_t{999})},
        {"name", persist::JsonValue(std::string("future"))},
    });
    CHECK(persist::migrateDocument(doc).isErr());
    CHECK(persist::projectFromJson(doc).isErr());
}

TEST_CASE("relink: resolve/store round-trip is lexical, no disk probing") {
    CHECK_EQ(persist::resolveAssetPath("C:/proj/sub/p.json", "media/a.mp4"),
             std::string("C:/proj/sub/media/a.mp4"));
    CHECK_EQ(persist::resolveAssetPath("C:/proj/p.json", "C:/media/a.mp4"),
             std::string("C:/media/a.mp4"));
    CHECK_EQ(persist::resolveAssetPath("p.json", "media/a.mp4"), std::string("media/a.mp4"));
    CHECK_EQ(persist::storeAssetPath("C:/proj/p.json", "C:/proj/media/a.mp4"),
             std::string("media/a.mp4"));
    CHECK_EQ(persist::storeAssetPath("C:/proj/p.json", "D:/media/a.mp4"),
             std::string("D:/media/a.mp4"));
    CHECK_EQ(persist::storeAssetPath("C:/proj/p.json", "media/a.mp4"),
             std::string("media/a.mp4"));
}

TEST_CASE("relink: save relativizes, load absolutizes, memory stays absolute") {
    namespace fs = std::filesystem;
    const fs::path dir = fs::current_path() / "relink_probe";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir / "media", ec);

    core::Project p;
    p.name = "relink";
    core::Asset a;
    a.id = "a1";
    a.path = (dir / "media" / "shot.mp4").lexically_normal().generic_string();
    a.duration = core::Rational(4, 1);
    p.assets.emplace(a.id, a);
    core::Sequence seq;
    seq.id = "s1";
    p.sequences.push_back(seq);
    p.activeSequenceId = "s1";

    const std::string projFile = (dir / "p.json").generic_string();
    CHECK(persist::saveProject(p, projFile).isOk());

    // On disk: relative reference (portable project folder).
    auto text = persist::readFile(projFile);
    CHECK(text.isOk());
    CHECK(text.value().find("media/shot.mp4") != std::string::npos);
    CHECK(text.value().find(a.path) == std::string::npos);

    // In memory after load: absolute (CWD-independent decoding).
    auto back = persist::loadProject(projFile);
    CHECK(back.isOk());
    CHECK_EQ(back.value().assets.at("a1").path, a.path);

    fs::remove_all(dir, ec);
}

TEST_CASE("migrations: v0 fps double migrates to rational") {
    persist::JsonValue doc(persist::JsonObject{
        {"schemaVersion", persist::JsonValue(std::int64_t{0})},
        {"name", persist::JsonValue(std::string("old"))},
        {"assets", persist::JsonValue(persist::JsonArray{})},
        {"sequences",
         persist::JsonValue(persist::JsonArray{
             persist::JsonValue(persist::JsonObject{
                 {"id", persist::JsonValue(std::string("s1"))},
                 {"fps", persist::JsonValue(30.0)},
                 {"tracks", persist::JsonValue(persist::JsonArray{})},
                 {"clips", persist::JsonValue(persist::JsonArray{})},
             }),
         })},
        {"activeSequenceId", persist::JsonValue(std::string("s1"))},
    });
    auto version = persist::migrateDocument(doc);
    CHECK(version.isOk());
    CHECK_EQ(version.value(), 2);
    auto project = persist::projectFromJson(doc);
    CHECK(project.isOk());
}

TEST_CASE("serialize: Stage 2 opacity and transitions round-trip") {
    core::Project p;
    p.name = "stage2";
    core::Sequence seq;
    seq.id = "s1";
    core::Track t;
    t.id = "v1";
    t.kind = core::TrackKind::Video;
    seq.tracks.push_back(t);

    core::Clip c1;
    c1.id = "c1";
    c1.name = "clip1";
    c1.opacity = 0.75;
    c1.sourceIn = core::Rational(0);
    c1.sourceOut = core::Rational(5, 1);
    c1.seqStart = core::Rational(0);

    core::Clip c2;
    c2.id = "c2";
    c2.name = "clip2";
    c2.opacity = 0.5;
    c2.sourceIn = core::Rational(0);
    c2.sourceOut = core::Rational(5, 1);
    c2.seqStart = core::Rational(5, 1);

    seq.clips.emplace(c1.id, c1);
    seq.clips.emplace(c2.id, c2);
    seq.tracks.front().clipIds.push_back(c1.id);
    seq.tracks.front().clipIds.push_back(c2.id);

    core::Transition tr;
    tr.id = "tr1";
    tr.trackId = "v1";
    tr.fromClipId = "c1";
    tr.toClipId = "c2";
    tr.type = "wipe_left";
    tr.duration = core::Rational(1, 1);
    tr.alignment = core::TransitionAlignment::CenterOnCut;
    tr.easing = "linear";
    seq.transitions.push_back(tr);

    p.sequences.push_back(seq);
    p.activeSequenceId = "s1";

    auto json = persist::projectToJson(p);
    CHECK(json.isOk());

    auto back = persist::projectFromJson(json.value());
    CHECK(back.isOk());
    const auto& s = back.value().sequences.front();
    CHECK_EQ(s.clips.at("c1").opacity, 0.75);
    CHECK_EQ(s.clips.at("c2").opacity, 0.5);
    CHECK_EQ(s.transitions.size(), 1);
    CHECK_EQ(s.transitions.front().id, std::string("tr1"));
    CHECK_EQ(s.transitions.front().type, std::string("wipe_left"));
    CHECK_EQ(s.transitions.front().alignment, core::TransitionAlignment::CenterOnCut);
}

TEST_CASE("serialize: clip speed and effects round-trip") {
    core::Project p;
    p.name = "effects_speed_test";
    core::Sequence seq;
    seq.id = "s1";
    core::Track t;
    t.id = "v1";
    t.kind = core::TrackKind::Video;
    seq.tracks.push_back(t);

    core::Clip c;
    c.id = "c1";
    c.name = "clip_with_effects";
    c.sourceIn = core::Rational(0);
    c.sourceOut = core::Rational(10, 1);
    c.seqStart = core::Rational(0);
    c.speed = core::Rational(3, 2); // 1.5x speed

    // Effect 1: color_adjust
    core::Effect e1;
    e1.type = "color_adjust";
    e1.enabled = true;
    e1.order = 0;
    e1.params["brightness"] = 0.2;
    e1.params["contrast"] = 1.1;
    e1.params["saturation"] = 1.3;
    c.effects.push_back(e1);

    // Effect 2: chroma_key with strParams
    core::Effect e2;
    e2.type = "chroma_key";
    e2.enabled = true;
    e2.order = 1;
    e2.params["similarity"] = 0.45;
    e2.params["smoothness"] = 0.15;
    e2.strParams["key_color"] = "#00FF00";
    c.effects.push_back(e2);

    seq.clips.emplace(c.id, c);
    seq.tracks.front().clipIds.push_back(c.id);
    p.sequences.push_back(seq);
    p.activeSequenceId = "s1";

    auto json = persist::projectToJson(p);
    CHECK(json.isOk());

    auto back = persist::projectFromJson(json.value());
    CHECK(back.isOk());

    const auto& roundtripClip = back.value().sequences.front().clips.at("c1");
    CHECK_EQ(roundtripClip.speed, core::Rational(3, 2));
    CHECK_EQ(roundtripClip.effects.size(), 2);

    // Verify e1
    CHECK_EQ(roundtripClip.effects[0].type, std::string("color_adjust"));
    CHECK_EQ(roundtripClip.effects[0].params.at("brightness"), 0.2);
    CHECK_EQ(roundtripClip.effects[0].params.at("contrast"), 1.1);
    CHECK_EQ(roundtripClip.effects[0].params.at("saturation"), 1.3);

    // Verify e2
    CHECK_EQ(roundtripClip.effects[1].type, std::string("chroma_key"));
    CHECK_EQ(roundtripClip.effects[1].params.at("similarity"), 0.45);
    CHECK_EQ(roundtripClip.effects[1].strParams.at("key_color"), std::string("#00FF00"));
}

int main() {
    return editor::tests::runAll();
}
