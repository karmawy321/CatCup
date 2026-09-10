#pragma once

// Cancellable offline-render job. Stage 0 walks the evaluator over the frame
// range into an IFrameSink (counting/null). Stage 1 adds an FFmpeg encoder
// sink behind the same interface — the job state machine does not change.

#include "core/Model.hpp"
#include "core/Rational.hpp"
#include "core/Result.hpp"
#include "render/Evaluator.hpp"

#include <atomic>
#include <cstdint>
#include <string>

namespace editor::exporter {

enum class ExportState { Idle, Running, Done, Cancelled, Failed };

struct ExportSettings {
    std::string outPath = "out.mp4";
    std::int64_t width = 1280;
    std::int64_t height = 720;
    core::Rational fps{30};
    std::string codec = "h264";
    int bitrateKbps = 8000;
};

class IFrameSink {
public:
    virtual ~IFrameSink() = default;
    /// Return false to abort the export (sink error).
    virtual bool onFrame(const render::FramePlan& plan, std::int64_t frameIndex) = 0;
};

class NullSink final : public IFrameSink {
public:
    bool onFrame(const render::FramePlan&, std::int64_t) override { return true; }
};

class CountingSink final : public IFrameSink {
public:
    bool onFrame(const render::FramePlan& plan, std::int64_t) override {
        ++frames;
        layers += static_cast<std::int64_t>(plan.layers.size());
        return true;
    }
    std::int64_t frames = 0;
    std::int64_t layers = 0;
};

class ExportJob {
public:
    explicit ExportJob(ExportSettings settings);

    void requestCancel() noexcept { cancelRequested_.store(true); }
    [[nodiscard]] ExportState state() const noexcept { return state_.load(); }
    [[nodiscard]] double progress() const noexcept { return progress_.load(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] const ExportSettings& settings() const noexcept { return settings_; }

    /// Synchronous run (worker-thread driven in Stage 1). Reports progress,
    /// honours requestCancel(), validates the project first.
    ExportState run(const core::Project& project, const core::Id& sequenceId,
                    IFrameSink& sink);

private:
    ExportSettings settings_;
    std::atomic<ExportState> state_{ExportState::Idle};
    std::atomic<double> progress_{0.0};
    std::atomic_bool cancelRequested_{false};
    std::string error_;
};

} // namespace editor::exporter
