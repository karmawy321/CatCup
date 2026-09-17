#include "TestHarness.hpp"

#include "persist/CapCutDraftBridge.hpp"
#include "persist/Json.hpp"
#include "persist/ProjectSerializer.hpp"
#include "core/Model.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>

using namespace editor;

namespace {

const char* kSampleDraftJson = R"({
  "canvas_config": {
    "width": 1920,
    "height": 1080,
    "ratio": "16:9"
  },
  "duration": 5000000,
  "fps": 30.0,
  "materials": {
    "videos": [
      {
        "id": "mat_v1",
        "path": "C:/fake/video.mp4",
        "duration": 10000000,
        "width": 1920,
        "height": 1080
      }
    ],
    "audios": [
      {
        "id": "mat_a1",
        "path": "C:/fake/audio.mp3",
        "duration": 5000000
      }
    ],
    "texts": [
      {
        "id": "mat_t1",
        "content": "Sample CapCut Title"
      }
    ],
    "speeds": [
      {
        "id": "spd_1",
        "speed": 2.0
      }
    ]
  },
  "tracks": [
    {
      "id": "track_video",
      "type": "video",
      "segments": [
        {
          "id": "seg_v1",
          "material_id": "mat_v1",
          "target_timerange": {
            "start": 0,
            "duration": 3000000
          },
          "source_timerange": {
            "start": 1000000,
            "duration": 6000000
          },
          "speed_id": "spd_1",
          "clip": {
            "scale": { "x": 1.25, "y": 1.25 },
            "transform": { "x": 10.0, "y": -20.0 },
            "rotation": 15.0,
            "alpha": 0.85
          }
        }
      ]
    },
    {
      "id": "track_audio",
      "type": "audio",
      "segments": [
        {
          "id": "seg_a1",
          "material_id": "mat_a1",
          "target_timerange": {
            "start": 500000,
            "duration": 4000000
          },
          "source_timerange": {
            "start": 0,
            "duration": 4000000
          }
        }
      ]
    },
    {
      "id": "track_text",
      "type": "text",
      "segments": [
        {
          "id": "seg_t1",
          "material_id": "mat_t1",
          "target_timerange": {
            "start": 1000000,
            "duration": 2000000
          }
        }
      ]
    }
  ]
})";

} // namespace

TEST_CASE("capcut_draft: import parses microsecond timings, tracks, and transforms") {
    const std::string tmpDraft = "test_capcut_sample_draft.json";
    {
        std::ofstream out(tmpDraft);
        out << kSampleDraftJson;
    }

    auto imported = persist::CapCutDraftBridge::importDraft(tmpDraft);
    std::filesystem::remove(tmpDraft);

    CHECK(imported.isOk());
    const auto& project = imported.value();
    const auto* seq = project.activeSequence();
    CHECK(seq != nullptr);

    // Canvas configuration
    CHECK_EQ(seq->width, 1920);
    CHECK_EQ(seq->height, 1080);
    CHECK_EQ(seq->fps, core::Rational(30, 1));

    // Tracks
    CHECK(seq->tracks.size() >= 3);
    
    // Check video clip
    auto itClip = seq->clips.find("seg_v1");
    CHECK(itClip != seq->clips.end());
    const auto& vClip = itClip->second;
    CHECK_EQ(vClip.seqStart, core::Rational(0, 1));
    CHECK_EQ(vClip.sourceIn, core::Rational(1, 1)); // 1,000,000 us = 1s
    CHECK_EQ(vClip.sourceOut, core::Rational(7, 1)); // 1s + 6s = 7s
    CHECK_EQ(vClip.speed, core::Rational(2, 1)); // speed 2.0
    CHECK(std::abs(vClip.transform.scale - 1.25) < 0.001);
    CHECK(std::abs(vClip.transform.x - 10.0) < 0.001);
    CHECK(std::abs(vClip.transform.y - (-20.0)) < 0.001);
    CHECK(std::abs(vClip.transform.rotationDeg - 15.0) < 0.001);
    CHECK(std::abs(vClip.opacity - 0.85) < 0.001);

    // Check audio clip
    auto itAudio = seq->clips.find("seg_a1");
    CHECK(itAudio != seq->clips.end());
    CHECK_EQ(itAudio->second.seqStart, core::Rational(1, 2)); // 500,000 us = 0.5s

    // Check text clip
    auto itText = seq->clips.find("seg_t1");
    CHECK(itText != seq->clips.end());
    CHECK_EQ(itText->second.text, std::string("Sample CapCut Title"));
    CHECK(itText->second.assetId.empty());
    CHECK_EQ(itText->second.seqStart, core::Rational(1, 1));
    CHECK_EQ(itText->second.seqDuration(), core::Rational(2, 1));
    CHECK_EQ(vClip.assetId, std::string("mat_v1"));
    CHECK_EQ(itAudio->second.assetId, std::string("mat_a1"));
    CHECK_EQ(project.assets.size(), 2);
    CHECK(project.validate().isOk());

    const std::string nativePath = "test_capcut_imported_project.json";
    CHECK(persist::saveProject(project, nativePath).isOk());
    auto reopened = persist::loadProject(nativePath);
    std::filesystem::remove(nativePath);
    CHECK(reopened.isOk());
    CHECK(reopened.value().validate().isOk());
    const auto* reopenedSeq = reopened.value().activeSequence();
    CHECK(reopenedSeq != nullptr);
    CHECK_EQ(reopenedSeq->id, seq->id);
    CHECK_EQ(reopenedSeq->clips.size(), seq->clips.size());
    const auto* reopenedText = reopenedSeq->findClip("seg_t1");
    CHECK(reopenedText != nullptr);
    CHECK(reopenedText->assetId.empty());
    CHECK_EQ(reopenedText->text, itText->second.text);
    CHECK_EQ(reopenedText->fontSizePt, itText->second.fontSizePt);
    CHECK_EQ(reopenedText->seqStart, itText->second.seqStart);
    CHECK_EQ(reopenedText->sourceIn, itText->second.sourceIn);
    CHECK_EQ(reopenedText->sourceOut, itText->second.sourceOut);
    CHECK_EQ(reopenedText->speed, itText->second.speed);
    const auto* reopenedTrack = reopenedSeq->findTrack("track_text");
    CHECK(reopenedTrack != nullptr);
    CHECK_EQ(reopenedTrack->kind, core::TrackKind::Text);
    CHECK_EQ(reopenedTrack->clipIds.size(), 1);
    CHECK_EQ(reopenedTrack->clipIds.front(), reopenedText->id);
    CHECK_EQ(reopenedSeq->clips.at("seg_v1").assetId, vClip.assetId);
    CHECK_EQ(reopenedSeq->clips.at("seg_a1").assetId, itAudio->second.assetId);
}

TEST_CASE("capcut_draft: export converts project to valid draft_content.json") {
    // Build a simple project
    core::Project p;
    p.name = "export-draft-test";
    core::Asset a;
    a.id = "asset-export";
    a.path = "C:/test/file.mp4";
    a.kind = core::AssetKind::Video;
    a.duration = core::Rational(10, 1);
    a.fps = core::Rational(30, 1);
    a.width = 1920;
    a.height = 1080;
    p.assets.emplace(a.id, a);

    core::Sequence seq;
    seq.id = "seq-1";
    seq.width = 1920;
    seq.height = 1080;
    seq.fps = core::Rational(30, 1);

    core::Track t;
    t.id = "v1";
    t.kind = core::TrackKind::Video;
    core::Clip c;
    c.id = "clip-export-1";
    c.assetId = a.id;
    c.name = "my-video";
    c.seqStart = core::Rational(1, 1);
    c.sourceIn = core::Rational(0, 1);
    c.sourceOut = core::Rational(4, 1);
    c.speed = core::Rational(1, 1);
    c.opacity = 0.9;
    c.transform.scale = 1.5;
    seq.clips.emplace(c.id, c);
    t.clipIds.push_back(c.id);
    seq.tracks.push_back(t);
    p.sequences.push_back(seq);
    p.activeSequenceId = seq.id;

    const std::string outPath = "test_capcut_exported_draft.json";
    auto expRes = persist::CapCutDraftBridge::exportDraft(p, outPath);
    CHECK(expRes.isOk());
    CHECK(std::filesystem::exists(outPath));

    // Verify it is parseable JSON
    std::string text;
    {
        std::ifstream in(outPath);
        text.assign((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
    }
    std::filesystem::remove(outPath);

    auto parsed = persist::parseJson(text);
    CHECK(parsed.isOk());
    const auto& doc = parsed.value();
    CHECK(doc.isObject());
    CHECK(doc.find("canvas_config") != nullptr);
    CHECK_EQ(doc.find("canvas_config")->find("width")->asInt(), 1920);
    CHECK_EQ(doc.find("canvas_config")->find("height")->asInt(), 1080);
    CHECK(doc.find("materials") != nullptr);
    CHECK(doc.find("materials")->find("videos") != nullptr);
    CHECK(doc.find("tracks") != nullptr);
}

int main() {
    return editor::tests::runAll();
}
