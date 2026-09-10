#include "media_ffmpeg/FfmpegProber.hpp"
#include "media_ffmpeg/FfmpegTime.hpp"

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace editor::media_ffmpeg {

core::Result<media::ProbeResult> FfmpegProber::probe(const std::string& path) {
    AVFormatContext* ctx = nullptr;
    if (avformat_open_input(&ctx, path.c_str(), nullptr, nullptr) < 0) {
        return core::Result<media::ProbeResult>::fail("cannot open media file: " + path);
    }
    // Balance every return with close: wrap before any early exit.
    struct Guard {
        AVFormatContext* c;
        ~Guard() {
            if (c != nullptr) {
                avformat_close_input(&c);
            }
        }
    } guard{ctx};

    if (avformat_find_stream_info(ctx, nullptr) < 0) {
        return core::Result<media::ProbeResult>::fail("cannot read stream info: " + path);
    }

    media::ProbeResult out;
    if (ctx->duration != AV_NOPTS_VALUE && ctx->duration > 0) {
        out.duration = core::Rational(ctx->duration, AV_TIME_BASE);
    }

    for (unsigned i = 0; i < ctx->nb_streams; ++i) {
        const AVStream* st = ctx->streams[i];
        const AVCodecParameters* par = st->codecpar;
        if (par->codec_type == AVMEDIA_TYPE_VIDEO && !out.hasVideo) {
            out.hasVideo = true;
            out.width = par->width;
            out.height = par->height;
            if (const AVCodec* codec = avcodec_find_decoder(par->codec_id)) {
                out.videoCodec = codec->name;
            }
            out.fps = ffmpeg_detail::fromAvRational(st->avg_frame_rate, core::Rational(30, 1));
            if (out.fps.num() <= 0) {
                out.fps =
                    ffmpeg_detail::fromAvRational(st->r_frame_rate, core::Rational(30, 1));
            }
            if (out.duration.isZero() && st->duration != AV_NOPTS_VALUE && st->duration > 0) {
                out.duration = ffmpeg_detail::ptsToSeconds(st->duration, st->time_base);
            }
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO && !out.hasAudio) {
            out.hasAudio = true;
            if (const AVCodec* codec = avcodec_find_decoder(par->codec_id)) {
                out.audioCodec = codec->name;
            }
            if (out.duration.isZero() && st->duration != AV_NOPTS_VALUE && st->duration > 0) {
                out.duration = ffmpeg_detail::ptsToSeconds(st->duration, st->time_base);
            }
        }
    }

    if (!out.hasVideo && !out.hasAudio) {
        return core::Result<media::ProbeResult>::fail("no audio or video streams: " + path);
    }
    return core::Result<media::ProbeResult>::ok(out);
}

} // namespace editor::media_ffmpeg
