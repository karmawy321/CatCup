#include "media_ffmpeg/FfmpegThumbnailer.hpp"
#include "media_ffmpeg/FrameDecoder.hpp"

extern "C" {
#include <libswscale/swscale.h>
}

namespace editor::media_ffmpeg {

core::Result<void> FfmpegThumbnailer::open(const std::string& path) {
    path_ = path;
    // Validate eagerly so a bad path fails at import, not at first paint.
    VideoDecoder probe;
    if (auto r = probe.open(path); r.isErr()) {
        open_ = false;
        return r;
    }
    open_ = true;
    return core::Result<void>::ok();
}

core::Result<ThumbnailImage> FfmpegThumbnailer::grab(const core::Rational& at,
                                                    int targetWidth) {
    using R = core::Result<ThumbnailImage>;
    if (!open_) {
        return R::fail("thumbnailer is not open");
    }
    if (targetWidth <= 0) {
        return R::fail("targetWidth must be positive");
    }
    VideoDecoder decoder;
    if (auto r = decoder.open(path_); r.isErr()) {
        return R::fail(r.error());
    }
    if (auto r = decoder.seek(at); r.isErr()) {
        return R::fail(r.error());
    }
    auto frame = decoder.nextFrame();
    if (frame.isErr()) {
        return R::fail(frame.error());
    }
    const DecodedVideoFrame& src = frame.value();
    ThumbnailImage out;
    out.width = targetWidth;
    out.height = (std::max)(1, src.height * targetWidth / (std::max)(1, src.width));
    out.rgba.resize(static_cast<std::size_t>(out.width) * out.height * 4);

    SwsContext* sws =
        sws_getContext(src.width, src.height, AV_PIX_FMT_RGBA, out.width, out.height,
                       AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (sws == nullptr) {
        return R::fail("cannot create thumbnail scaler");
    }
    const std::uint8_t* srcPlanes[4] = {src.rgba.data(), nullptr, nullptr, nullptr};
    int srcStride[4] = {src.width * 4, 0, 0, 0};
    std::uint8_t* dstPlanes[4] = {out.rgba.data(), nullptr, nullptr, nullptr};
    int dstStride[4] = {out.width * 4, 0, 0, 0};
    sws_scale(sws, srcPlanes, srcStride, 0, src.height, dstPlanes, dstStride);
    sws_freeContext(sws);
    return R::ok(std::move(out));
}

} // namespace editor::media_ffmpeg
