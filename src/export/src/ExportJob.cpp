#include "export/ExportJob.hpp"

#include <cmath>

namespace editor::exporter {

ExportJob::ExportJob(ExportSettings settings) : settings_(std::move(settings)) {}

ExportState ExportJob::run(const core::Project& project, const core::Id& sequenceId,
                           IFrameSink& sink) {
    state_.store(ExportState::Running);
    progress_.store(0.0);
    error_.clear();

    const core::Sequence* seq = nullptr;
    for (const auto& s : project.sequences) {
        if (s.id == sequenceId) {
            seq = &s;
            break;
        }
    }
    if (seq == nullptr) {
        error_ = "sequence not found: " + sequenceId;
        state_.store(ExportState::Failed);
        return state_.load();
    }
    if (project.validate().isErr()) {
        error_ = "cannot export invalid project: " + project.validate().error();
        state_.store(ExportState::Failed);
        return state_.load();
    }
    if (!(settings_.fps.num() > 0)) {
        error_ = "export fps must be positive";
        state_.store(ExportState::Failed);
        return state_.load();
    }

    const core::Rational duration = render::Evaluator::sequenceDuration(*seq);
    if (duration.isZero() || duration.isNegative()) {
        error_ = "nothing to export: sequence is empty";
        state_.store(ExportState::Failed);
        return state_.load();
    }
    const std::int64_t totalFrames = duration.toFramesRounded(settings_.fps);
    if (totalFrames <= 0) {
        error_ = "nothing to export: duration rounds to zero frames";
        state_.store(ExportState::Failed);
        return state_.load();
    }

    for (std::int64_t i = 0; i < totalFrames; ++i) {
        if (cancelRequested_.load()) {
            state_.store(ExportState::Cancelled);
            return state_.load();
        }
        const core::Rational t = core::Rational::fromFrames(i, settings_.fps);
        const render::FramePlan plan = render::Evaluator::evaluateVideoAt(*seq, t);
        if (!sink.onFrame(plan, i)) {
            error_ = "frame sink failed at frame " + std::to_string(i);
            state_.store(ExportState::Failed);
            return state_.load();
        }
        progress_.store(static_cast<double>(i + 1) / static_cast<double>(totalFrames));
    }

    progress_.store(1.0);
    state_.store(ExportState::Done);
    return state_.load();
}

} // namespace editor::exporter
