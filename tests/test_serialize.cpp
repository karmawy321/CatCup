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
    CHECK_EQ(version.value(), 1);
    auto project = persist::projectFromJson(doc);
    CHECK(project.isOk());
}

int main() {
    return editor::tests::runAll();
}
