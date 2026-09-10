#include "TestHarness.hpp"

#include "core/Model.hpp"
#include "export/ExportJob.hpp"

using namespace editor;

namespace {
core::Project makeProject() {
    core::Project p;
    p.name = "export";
    core::Asset a;
    a.id = "asset-1";
    a.duration = core::Rational(8, 1);
    a.fps = core::Rational(30, 1);
    p.assets.emplace(a.id, a);
    core::Sequence seq;
    seq.id = "seq-1";
    seq.fps = core::Rational(30, 1);
    core::Track t;
    t.id = "v1";
    t.kind = core::TrackKind::Video;
    seq.tracks.push_back(t);
    core::Clip c;
    c.id = "c1";
    c.assetId = "asset-1";
    c.sourceIn = core::Rational(0);
    c.sourceOut = core::Rational(2, 1); // 2 s @ 30 fps == 60 frames
    c.seqStart = core::Rational(0);
    seq.clips.emplace(c.id, c);
    seq.tracks.front().clipIds.push_back(c.id);
    p.sequences.push_back(seq);
    p.activeSequenceId = "seq-1";
    return p;
}
} // namespace

TEST_CASE("export: frame count matches duration x fps") {
    const core::Project p = makeProject();
    exporter::ExportSettings settings;
    exporter::ExportJob job(settings);
    exporter::CountingSink sink;
    CHECK(job.run(p, "seq-1", sink) == exporter::ExportState::Done);
    CHECK_EQ(sink.frames, 60);
    CHECK_EQ(job.progress(), 1.0);
}

TEST_CASE("export: cancel stops the run") {
    struct CancellingSink final : exporter::IFrameSink {
        exporter::ExportJob* job = nullptr;
        bool onFrame(const render::FramePlan&, std::int64_t i) override {
            if (i == 5) {
                job->requestCancel();
            }
            return true;
        }
    };
    const core::Project p = makeProject();
    exporter::ExportJob job(exporter::ExportSettings{});
    CancellingSink sink;
    sink.job = &job;
    CHECK(job.run(p, "seq-1", sink) == exporter::ExportState::Cancelled);
}

TEST_CASE("export: empty sequence fails with a useful message") {
    core::Project p = makeProject();
    p.sequences.front().clips.clear();
    p.sequences.front().tracks.front().clipIds.clear();
    exporter::ExportJob job(exporter::ExportSettings{});
    exporter::CountingSink sink;
    CHECK(job.run(p, "seq-1", sink) == exporter::ExportState::Failed);
    CHECK(!job.error().empty());
}

TEST_CASE("export: unknown sequence fails with a useful message") {
    const core::Project p = makeProject();
    exporter::ExportJob job(exporter::ExportSettings{});
    exporter::CountingSink sink;
    CHECK(job.run(p, "nope", sink) == exporter::ExportState::Failed);
    CHECK(!job.error().empty());
}

int main() {
    return editor::tests::runAll();
}
