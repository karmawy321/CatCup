#include "TestHarness.hpp"

#include "ai/JobSystem.hpp"

using namespace editor;

TEST_CASE("jobs: submit, step, finish with provenance") {
    ai::JobQueue q;
    ai::JobSpec spec;
    spec.type = "transcribe";
    spec.inputHash = "sha:abc";
    spec.modelVersion = "whisper-1";
    const core::Id id = q.submit(spec);
    CHECK(!id.empty());

    q.step(id, 0.5);
    const auto mid = q.status(id);
    CHECK(mid.has_value());
    CHECK(mid->state == ai::JobState::Running);

    ai::DerivedAsset out;
    out.sourceHash = "sha:abc";
    out.algorithm = "transcribe";
    out.algorithmVersion = "whisper-1";
    out.paramsHash = "p:1";
    out.path = "cache/transcribe-abc.json";
    q.finish(id, out);
    const auto done = q.status(id);
    CHECK(done.has_value() && done->state == ai::JobState::Done);
    const auto res = q.result(id);
    CHECK(res.has_value() && res->valid);
    CHECK_EQ(res->sourceHash, std::string("sha:abc"));
}

TEST_CASE("jobs: cancel wins over finish") {
    ai::JobQueue q;
    ai::JobSpec spec;
    spec.type = "upscale";
    const core::Id id = q.submit(spec);
    CHECK(q.cancel(id));
    ai::DerivedAsset out;
    q.finish(id, out); // must be ignored — cancelled stays cancelled
    CHECK(q.status(id)->state == ai::JobState::Cancelled);
    CHECK(!q.result(id).has_value());
}

TEST_CASE("jobs: failure carries a message") {
    ai::JobQueue q;
    ai::JobSpec spec;
    spec.type = "segment";
    const core::Id id = q.submit(spec);
    q.fail(id, "out of GPU memory");
    const auto st = q.status(id);
    CHECK(st.has_value() && st->state == ai::JobState::Failed);
    CHECK_EQ(st->error, std::string("out of GPU memory"));
}

int main() {
    return editor::tests::runAll();
}
