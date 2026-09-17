#pragma once

// Software frame composer for preview: evaluator -> decode -> fit-to-canvas
// -> title overlay, painted with QPainter. One instance per thread
// (decoders are not thread-safe): PreviewItem owns one for scrubbing, the
// Player worker owns one for playback. Export uses the filter-graph path
// over the SAME inputs and layout math (center + offset, px font size) —
// the pixel-variance export test guards their agreement until S2 unifies
// them behind one CPU reference compositor.

#include "core/Model.hpp"
#include "core/Rational.hpp"
#include "media_ffmpeg/FrameDecoder.hpp"

#include <QImage>
#include <QSize>

#include <map>
#include <optional>

namespace editor::shell {

class FrameCompositor {
public:
    QImage frameAt(const core::Project& project, const core::Sequence& seq,
                   const core::Rational& t);
    void invalidate(); // call after any project mutation

private:
    struct Reader {
        std::string path;
        media_ffmpeg::VideoDecoder decoder;
        core::Rational lastDelivered{-1, 1};
        std::optional<media_ffmpeg::DecodedVideoFrame> cachedFrame;
        bool ready = false;
    };
    std::map<core::Id, Reader> readers_;
};

} // namespace editor::shell
