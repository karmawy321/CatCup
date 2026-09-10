#include "ai/JobSystem.hpp"

namespace editor::ai {

core::Id JobQueue::submit(JobSpec spec) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (spec.id.empty()) {
        spec.id = core::IdGenerator::make("job");
    }
    const core::Id id = spec.id;
    specs_.emplace(id, std::move(spec));
    states_.emplace(id, JobStatus{JobState::Queued, 0.0, {}});
    return id;
}

bool JobQueue::cancel(const core::Id& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = states_.find(id);
    if (it == states_.end()) {
        return false;
    }
    if (it->second.state == JobState::Done || it->second.state == JobState::Failed) {
        return false;
    }
    it->second.state = JobState::Cancelled;
    return true;
}

void JobQueue::step(const core::Id& id, double progressDelta) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = states_.find(id);
    if (it == states_.end()) {
        return;
    }
    if (it->second.state == JobState::Cancelled || it->second.state == JobState::Done ||
        it->second.state == JobState::Failed) {
        return;
    }
    it->second.state = JobState::Running;
    it->second.progress += progressDelta;
    if (it->second.progress >= 1.0) {
        it->second.progress = 1.0;
    }
}

void JobQueue::finish(const core::Id& id, DerivedAsset asset) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = states_.find(id);
    if (it == states_.end()) {
        return;
    }
    if (it->second.state == JobState::Cancelled) {
        return;
    }
    asset.valid = true;
    results_[id] = std::move(asset);
    it->second.state = JobState::Done;
    it->second.progress = 1.0;
}

void JobQueue::fail(const core::Id& id, std::string error) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = states_.find(id);
    if (it == states_.end()) {
        return;
    }
    it->second.state = JobState::Failed;
    it->second.error = std::move(error);
}

std::optional<JobStatus> JobQueue::status(const core::Id& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = states_.find(id);
    if (it == states_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<DerivedAsset> JobQueue::result(const core::Id& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = results_.find(id);
    if (it == results_.end()) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace editor::ai
