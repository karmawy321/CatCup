#pragma once

// Real MP4 export (libx264 video + AAC audio) driven by the SAME
// render::Evaluator the preview uses: each output frame asks the evaluator
// which source frames are active, decodes them, composites through a small
// libavfilter graph (canvas fit + title overlays), and encodes.
//
// Stage 1 scope: single video layer wins if tracks overlap (multitrack blend
// is Stage 2); titles render centered plus clip offset at fontSizePt pixels;
// audio is the concatenated audio-track clips with digital silence in gaps.
// Anything outside this fails loudly instead of degrading silently.

#include "core/Ids.hpp"
#include "core/Model.hpp"
#include "core/Result.hpp"

#include <atomic>
#include <functional>
#include <string>

namespace editor::export_ffmpeg {

struct Mp4ExportOptions {
    std::string outPath = "out.mp4";
    int videoBitrateKbps = 8000;
    int audioBitrateKbps = 160;
    std::string x264Preset = "veryfast";
};

class Mp4Exporter {
public:
    using ProgressFn = std::function<void(double)>; // 0..1, video 0-0.9, audio 0.9-1

    /// Canvas/fps come from the sequence (single source of truth).
    /// Returns ok() on completion; "cancelled" Result on requestCancel.
    /// Partial output is removed on failure/cancel.
    core::Result<void> run(const core::Project& project, const core::Id& sequenceId,
                           const Mp4ExportOptions& options, const std::atomic_bool& cancel,
                           ProgressFn progress = {});
};

} // namespace editor::export_ffmpeg
