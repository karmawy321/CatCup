#include "media_ffmpeg/FrameDecoder.hpp"
#include "media_ffmpeg/FfmpegTime.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <cstring>

namespace editor::media_ffmpeg {
namespace {

constexpr const char* kEof = "eof";

core::Rational framePtsSeconds(const AVFrame* frame, const AVStream* stream) {
    const std::int64_t ts = frame->best_effort_timestamp;
    if (ts == AV_NOPTS_VALUE) {
        return core::Rational(0);
    }
    return ffmpeg_detail::ptsToSeconds(ts, stream->time_base);
}

} // namespace

struct VideoDecoder::Impl {
    AVFormatContext* fmt = nullptr;
    AVCodecContext* codec = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    SwsContext* sws = nullptr;
    int streamIndex = -1;
    const AVStream* stream = nullptr;
    bool eof = false;
    bool haveTarget = false;
    core::Rational target{0};

    ~Impl() {
        if (sws != nullptr) {
            sws_freeContext(sws);
        }
        if (frame != nullptr) {
            av_frame_free(&frame);
        }
        if (packet != nullptr) {
            av_packet_free(&packet);
        }
        if (codec != nullptr) {
            avcodec_free_context(&codec);
        }
        if (fmt != nullptr) {
            avformat_close_input(&fmt);
        }
    }
};

VideoDecoder::VideoDecoder() : impl_(std::make_unique<Impl>()) {}
VideoDecoder::~VideoDecoder() = default;

core::Result<void> VideoDecoder::open(const std::string& path) {
    impl_ = std::make_unique<Impl>();
    Impl& im = *impl_;

    if (avformat_open_input(&im.fmt, path.c_str(), nullptr, nullptr) < 0) {
        return core::Result<void>::fail("cannot open media file: " + path);
    }
    if (avformat_find_stream_info(im.fmt, nullptr) < 0) {
        return core::Result<void>::fail("cannot read stream info: " + path);
    }
    int best = av_find_best_stream(im.fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (best < 0) {
        return core::Result<void>::fail("no video stream: " + path);
    }
    im.streamIndex = best;
    im.stream = im.fmt->streams[best];
    const AVCodecParameters* par = im.stream->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(par->codec_id);
    if (decoder == nullptr) {
        return core::Result<void>::fail("unsupported video codec in: " + path);
    }
    im.codec = avcodec_alloc_context3(decoder);
    if (im.codec == nullptr) {
        return core::Result<void>::fail("cannot allocate video decoder");
    }
    if (avcodec_parameters_to_context(im.codec, par) < 0) {
        return core::Result<void>::fail("cannot configure video decoder");
    }
    // Stage 1: single-threaded decode is fast enough for 720p preview and
    // keeps frame order trivially correct. Threading is an S2 optimization.
    if (avcodec_open2(im.codec, decoder, nullptr) < 0) {
        return core::Result<void>::fail("cannot open video decoder");
    }
    im.frame = av_frame_alloc();
    im.packet = av_packet_alloc();
    if (im.frame == nullptr || im.packet == nullptr) {
        return core::Result<void>::fail("cannot allocate video decode buffers");
    }

    width_ = par->width;
    height_ = par->height;
    fps_ = ffmpeg_detail::fromAvRational(im.stream->avg_frame_rate, core::Rational(30, 1));
    if (fps_.num() <= 0) {
        fps_ = ffmpeg_detail::fromAvRational(im.stream->r_frame_rate, core::Rational(30, 1));
    }
    if (im.fmt->duration != AV_NOPTS_VALUE && im.fmt->duration > 0) {
        duration_ = core::Rational(im.fmt->duration, AV_TIME_BASE);
    }
    open_ = true;
    return core::Result<void>::ok();
}

core::Result<void> VideoDecoder::seek(const core::Rational& t) {
    if (!open_) {
        return core::Result<void>::fail("video decoder is not open");
    }
    Impl& im = *impl_;
    const std::int64_t ts = ffmpeg_detail::secondsToAvTime(t < core::Rational(0) ? core::Rational(0) : t);
    if (av_seek_frame(im.fmt, -1, ts, AVSEEK_FLAG_BACKWARD) < 0) {
        return core::Result<void>::fail("seek failed");
    }
    avcodec_flush_buffers(im.codec);
    im.eof = false;
    im.haveTarget = true;
    im.target = t;
    return core::Result<void>::ok();
}

core::Result<DecodedVideoFrame> VideoDecoder::nextFrame() {
    using R = core::Result<DecodedVideoFrame>;
    if (!open_) {
        return R::fail("video decoder is not open");
    }
    Impl& im = *impl_;

    auto convertCurrent = [&]() -> core::Result<DecodedVideoFrame> {
        using R = core::Result<DecodedVideoFrame>;
        if (im.sws == nullptr) {
            im.sws = sws_getContext(im.codec->width, im.codec->height,
                                    im.codec->pix_fmt, im.codec->width, im.codec->height,
                                    AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
            if (im.sws == nullptr) {
                return R::fail("cannot create pixel converter");
            }
        }
        DecodedVideoFrame out;
        out.width = im.codec->width;
        out.height = im.codec->height;
        out.pts = framePtsSeconds(im.frame, im.stream);
        out.rgba.resize(static_cast<std::size_t>(out.width) * out.height * 4);
        std::uint8_t* dst[4] = {out.rgba.data(), nullptr, nullptr, nullptr};
        int stride[4] = {out.width * 4, 0, 0, 0};
        sws_scale(im.sws, im.frame->data, im.frame->linesize, 0, im.codec->height, dst,
                  stride);
        return R::ok(std::move(out));
    };

    // Drain any frames already queued in the codec first.
    while (true) {
        int rc = avcodec_receive_frame(im.codec, im.frame);
        if (rc == 0) {
            auto converted = convertCurrent();
            if (converted.isErr()) {
                return converted;
            }
            DecodedVideoFrame out = std::move(converted.value());
            if (im.haveTarget && out.pts < im.target) {
                continue; // drop pre-seek frame
            }
            im.haveTarget = false;
            return R::ok(std::move(out));
        }
        if (rc != AVERROR(EAGAIN)) {
            break; // EOF or error: fall through to packet feeding/flush
        }
        // Need more packets.
        rc = av_read_frame(im.fmt, im.packet);
        if (rc < 0) {
            if (rc == AVERROR_EOF) {
                im.eof = true;
            }
            avcodec_send_packet(im.codec, nullptr); // flush
            continue;
        }
        if (im.packet->stream_index != im.streamIndex) {
            av_packet_unref(im.packet);
            continue;
        }
        rc = avcodec_send_packet(im.codec, im.packet);
        av_packet_unref(im.packet);
        if (rc < 0 && rc != AVERROR(EAGAIN)) {
            return R::fail("video decode failed: " + ffmpeg_detail::avErrorString(rc));
        }
    }

    if (im.eof) {
        return R::fail(kEof);
    }
    return R::fail("video decode failed");
}

} // namespace editor::media_ffmpeg
