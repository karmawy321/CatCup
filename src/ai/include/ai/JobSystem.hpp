#pragma once

// AI/provider job system (Stages 4-6 shape, Stage 0 skeleton).
// Jobs are immutable specs with progress + cancellation + provenance.
// Derived assets record (sourceHash, algorithm, version, params) so cached
// results invalidate correctly when inputs or models change.

#include "core/Ids.hpp"
#include "core/Result.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace editor::ai {

enum class JobState { Queued, Running, Done, Cancelled, Failed };

struct JobSpec {
    core::Id id;
    std::string type; // e.g. "transcribe", "segment", "upscale"
    std::string inputHash;
    std::map<std::string, std::string> params;
    std::string modelVersion = "none";
};

struct JobStatus {
    JobState state = JobState::Queued;
    double progress = 0.0; // 0..1
    std::string error;
};

struct DerivedAsset {
    std::string sourceHash;
    std::string algorithm;
    std::string algorithmVersion;
    std::string paramsHash;
    std::string path; // cache location
    bool valid = false;
};

class JobQueue {
public:
    core::Id submit(JobSpec spec);
    bool cancel(const core::Id& id);
    /// Deterministic manual stepping for tests; worker threads arrive later.
    void step(const core::Id& id, double progressDelta);
    void finish(const core::Id& id, DerivedAsset asset);
    void fail(const core::Id& id, std::string error);
    [[nodiscard]] std::optional<JobStatus> status(const core::Id& id) const;
    [[nodiscard]] std::optional<DerivedAsset> result(const core::Id& id) const;

private:
    mutable std::mutex mutex_;
    std::map<core::Id, JobSpec> specs_;
    std::map<core::Id, JobStatus> states_;
    std::map<core::Id, DerivedAsset> results_;
};

} // namespace editor::ai
