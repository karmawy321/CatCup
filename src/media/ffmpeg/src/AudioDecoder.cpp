#include "media_ffmpeg/FrameDecoder.hpp"
#include "media_ffmpeg/FfmpegTime.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

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

struct AudioDecoder::Impl {
    AVFormatContext* fmt = nullptr;
    AVCodecContext* codec = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    SwrContext* swr = nullptr;
    int streamIndex = -1;
    const AVStream* stream = nullptr;
    bool eof = false;
    bool haveTarget = false;
    core::Rational target{0};
    // Pending converted samples when a decoded frame exceeds the request.
    std::vector<std::int16_t> pending;
    core::Rational pendingPts{0};

    ~Impl() {
        if (swr != nullptr) {
            swr_free(&swr);
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

AudioDecoder::AudioDecoder() : impl_(std::make_unique<Impl>()) {}
AudioDecoder::~AudioDecoder() = default;

core::Result<void> AudioDecoder::open(const std::string& path) {
    impl_ = std::make_unique<Impl>();
    Impl& im = *impl_;

    if (avformat_open_input(&im.fmt, path.c_str(), nullptr, nullptr) < 0) {
        return core::Result<void>::fail("cannot open media file: " + path);
    }
    if (avformat_find_stream_info(im.fmt, nullptr) < 0) {
        return core::Result<void>::fail("cannot read stream info: " + path);
    }
    const int best = av_find_best_stream(im.fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (best < 0) {
        hasAudio_ = false; // video-only file: not an error, just silent
        return core::Result<void>::ok();
    }
    im.streamIndex = best;
    im.stream = im.fmt->streams[best];
    const AVCodecParameters* par = im.stream->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(par->codec_id);
    if (decoder == nullptr) {
        return core::Result<void>::fail("unsupported audio codec in: " + path);
    }
    im.codec = avcodec_alloc_context3(decoder);
    if (im.codec == nullptr) {
        return core::Result<void>::fail("cannot allocate audio decoder");
    }
    if (avcodec_parameters_to_context(im.codec, par) < 0) {
        return core::Result<void>::fail("cannot configure audio decoder");
    }
    if (avcodec_open2(im.codec, decoder, nullptr) < 0) {
        return core::Result<void>::fail("cannot open audio decoder");
    }
    im.frame = av_frame_alloc();
    im.packet = av_packet_alloc();
    if (im.frame == nullptr || im.packet == nullptr) {
        return core::Result<void>::fail("cannot allocate audio decode buffers");
    }

    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, DecodedAudioChunk::kChannels);
    if (swr_alloc_set_opts2(&im.swr, &outLayout, AV_SAMPLE_FMT_S16,
                            DecodedAudioChunk::kSampleRate, &im.codec->ch_layout,
                            im.codec->sample_fmt, im.codec->sample_rate, 0, nullptr) < 0) {
        av_channel_layout_uninit(&outLayout);
        return core::Result<void>::fail("cannot configure audio resampler");
    }
    av_channel_layout_uninit(&outLayout);
    if (swr_init(im.swr) < 0) {
        return core::Result<void>::fail("cannot initialize audio resampler");
    }
    hasAudio_ = true;
    return core::Result<void>::ok();
}

core::Result<void> AudioDecoder::seek(const core::Rational& t) {
    if (!hasAudio_) {
        return core::Result<void>::ok(); // silent track: nothing to seek
    }
    Impl& im = *impl_;
    const std::int64_t ts =
        ffmpeg_detail::secondsToAvTime(t < core::Rational(0) ? core::Rational(0) : t);
    int ret = -1;
    if (im.streamIndex >= 0 && im.stream != nullptr) {
        const int64_t streamTs = av_rescale_q(ts, AVRational{1, AV_TIME_BASE}, im.stream->time_base);
        ret = av_seek_frame(im.fmt, im.streamIndex, streamTs, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            ret = av_seek_frame(im.fmt, im.streamIndex, streamTs, 0);
        }
    }
    if (ret < 0) {
        ret = av_seek_frame(im.fmt, -1, ts, AVSEEK_FLAG_BACKWARD);
    }
    if (ret < 0) {
        ret = av_seek_frame(im.fmt, -1, ts, 0);
    }
    if (ret < 0 && ts > 0) {
        ret = av_seek_frame(im.fmt, -1, 0, AVSEEK_FLAG_BACKWARD);
    }
    if (ret < 0) {
        return core::Result<void>::fail("audio seek failed");
    }
    avcodec_flush_buffers(im.codec);
    im.eof = false;
    im.haveTarget = true;
    im.target = t;
    im.pending.clear();
    return core::Result<void>::ok();
}

core::Result<DecodedAudioChunk> AudioDecoder::nextChunk(int maxSamplesPerChannel) {
    using R = core::Result<DecodedAudioChunk>;
    if (!hasAudio_) {
        return R::fail(kEof);
    }
    if (maxSamplesPerChannel <= 0) {
        return R::fail("maxSamplesPerChannel must be positive");
    }
    Impl& im = *impl_;
    DecodedAudioChunk out;
    out.startPts = im.pendingPts;
    bool started = !im.pending.empty();

    // Convert one decoded frame into the canonical format, appending to out.
    auto convertFrame = [&]() -> core::Result<void> {
        const core::Rational framePts = framePtsSeconds(im.frame, im.stream);
        if (im.haveTarget) {
            // Drop whole frames ending at or before the target.
            const double frameDur =
                im.frame->nb_samples > 0 && im.codec->sample_rate > 0
                    ? static_cast<double>(im.frame->nb_samples) / im.codec->sample_rate
                    : 0.0;
            const double frameEnd =
                static_cast<double>(framePts) + frameDur - 1e-6; // tolerance
            if (frameEnd <= static_cast<double>(im.target)) {
                return core::Result<void>::ok(); // dropped
            }
            im.haveTarget = false; // first overlapping frame: keep (S1 tolerance)
        }
        const int maxOut = swr_get_out_samples(im.swr, im.frame->nb_samples) + 256;
        std::vector<std::int16_t> tmp(static_cast<std::size_t>(maxOut) *
                                      DecodedAudioChunk::kChannels);
        std::uint8_t* dst = reinterpret_cast<std::uint8_t*>(tmp.data());
        const int converted =
            swr_convert(im.swr, &dst, maxOut,
                        im.frame->extended_data == nullptr
                            ? nullptr
                            : const_cast<const std::uint8_t**>(im.frame->extended_data),
                        im.frame->nb_samples);
        if (converted < 0) {
            return core::Result<void>::fail("audio resample failed");
        }
        tmp.resize(static_cast<std::size_t>(converted) * DecodedAudioChunk::kChannels);
        if (!started) {
            out.startPts = framePts;
            started = true;
        }
        out.pcm.insert(out.pcm.end(), tmp.begin(), tmp.end());
        return core::Result<void>::ok();
    };

    // Serve leftovers first.
    if (!im.pending.empty()) {
        const std::size_t want = static_cast<std::size_t>(maxSamplesPerChannel) *
                                 DecodedAudioChunk::kChannels;
        const std::size_t take = (std::min)(want, im.pending.size());
        out.pcm.insert(out.pcm.end(), im.pending.begin(), im.pending.begin() + take);
        im.pending.erase(im.pending.begin(), im.pending.begin() + take);
        if (static_cast<int>(out.pcm.size() / DecodedAudioChunk::kChannels) >=
            maxSamplesPerChannel) {
            return R::ok(std::move(out));
        }
    }

    while (static_cast<int>(out.pcm.size() / DecodedAudioChunk::kChannels) <
           maxSamplesPerChannel) {
        const int rc = avcodec_receive_frame(im.codec, im.frame);
        if (rc == 0) {
            if (auto r = convertFrame(); r.isErr()) {
                return R::fail(r.error());
            }
            continue;
        }
        if (rc != AVERROR(EAGAIN)) {
            break;
        }
        const int readRc = av_read_frame(im.fmt, im.packet);
        if (readRc < 0) {
            if (readRc == AVERROR_EOF) {
                im.eof = true;
            }
            avcodec_send_packet(im.codec, nullptr);
            continue;
        }
        if (im.packet->stream_index != im.streamIndex) {
            av_packet_unref(im.packet);
            continue;
        }
        const int sendRc = avcodec_send_packet(im.codec, im.packet);
        av_packet_unref(im.packet);
        if (sendRc < 0 && sendRc != AVERROR(EAGAIN)) {
            return R::fail("audio decode failed: " + ffmpeg_detail::avErrorString(sendRc));
        }
    }

    // Stash overflow for the next call so chunk boundaries stay exact.
    const std::size_t want = static_cast<std::size_t>(maxSamplesPerChannel) *
                             DecodedAudioChunk::kChannels;
    if (out.pcm.size() > want) {
        im.pending.assign(out.pcm.begin() + want, out.pcm.end());
        // Advance the pending start by the consumed audio duration.
        const double consumedSec = static_cast<double>(maxSamplesPerChannel) /
                                   DecodedAudioChunk::kSampleRate;
        const auto consumedMicros =
            static_cast<std::int64_t>(consumedSec * 1000000.0 + 0.5);
        im.pendingPts = out.startPts + core::Rational(consumedMicros, 1000000);
        out.pcm.resize(want);
    }
    if (out.pcm.empty() && im.eof) {
        return R::fail(kEof);
    }
    return R::ok(std::move(out));
}

} // namespace editor::media_ffmpeg
