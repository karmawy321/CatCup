#include "export_ffmpeg/Mp4Exporter.hpp"

#include "media_ffmpeg/FfmpegTime.hpp"
#include "media_ffmpeg/FrameDecoder.hpp"
#include "render/Evaluator.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <utility>
#include <vector>

namespace editor::export_ffmpeg {
namespace {

constexpr const char* kFontFile = "C:/Windows/Fonts/arial.ttf";

std::string formatDouble(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6f", v);
    return buf;
}

// Titles become UTF-8 sibling files referenced via textfile= (no quoting
// hell for CJK/quotes/emoji in filter strings). Caller cleans them up.
core::Result<std::string> writeTitleFile(const std::string& dir, int index,
                                         const std::string& text) {
    const std::string path = dir + "/editor_title_" + std::to_string(index) + ".txt";
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || f == nullptr) {
        return core::Result<std::string>::fail("cannot write title temp file");
    }
    // UTF-8 BOM helps some Windows text readers; harmless to freetype.
    const unsigned char bom[3] = {0xEF, 0xBB, 0xBF};
    std::fwrite(bom, 1, 3, f);
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    return core::Result<std::string>::ok(path);
}

struct TitleDef {
    std::string file;
    double startSec = 0.0;
    double endSec = 0.0;
    int fontPx = 48;
    int dx = 0;
    int dy = 0;
};

// Create one filter instance or fail with its name (no string-graph parsing:
// Windows drive-letter colons do not survive avfilter_graph_parse_ptr even
// escaped in this FFmpeg 8.1 build, so every filter is created and linked
// programmatically and paths travel via av_opt_set, which needs no escaping).
// Create one filter instance or fail with its name (no string-graph parsing:
// Windows drive-letter colons do not survive avfilter_graph_parse_ptr even
// escaped in this FFmpeg 8.1 build, so every filter is created and linked
// programmatically and paths travel via av_opt_set, which needs no escaping).
//
// Two-phase init (alloc -> set options -> init) matters: drawtext validates
// text/textfile in its init callback, which avfilter_graph_create_filter
// would run BEFORE we could set anything.
AVFilterContext* makeFilter(AVFilterGraph* graph, const char* name, const char* instance,
                            const char* args,
                            const std::vector<std::pair<std::string, std::string>>* options,
                            std::string& error) {
    const AVFilter* def = avfilter_get_by_name(name);
    if (def == nullptr) {
        error = std::string("filter not in pinned FFmpeg: ") + name;
        return nullptr;
    }
    AVFilterContext* ctx = avfilter_graph_alloc_filter(graph, def, instance);
    if (ctx == nullptr) {
        error = std::string("cannot allocate filter: ") + name;
        return nullptr;
    }
    if (options != nullptr) {
        for (const auto& [key, value] : *options) {
            // SEARCH_CHILDREN: drawtext's options live on its private class,
            // not on the filter context itself.
            if (av_opt_set(ctx, key.c_str(), value.c_str(), AV_OPT_SEARCH_CHILDREN) < 0) {
                avfilter_free(ctx);
                error = std::string("cannot set ") + name + "." + key;
                return nullptr;
            }
        }
    }
    const int rc = (args != nullptr) ? avfilter_init_str(ctx, args)
                                     : avfilter_init_dict(ctx, nullptr);
    if (rc < 0) {
        avfilter_free(ctx);
        error = std::string("cannot init filter: ") + name + " (" +
                ffmpeg_detail::avErrorString(rc) + ")";
        return nullptr;
    }
    return ctx;
}

struct Encoder {
    AVCodecContext* ctx = nullptr;
    AVStream* stream = nullptr;
    int64_t nextPts = 0;
    ~Encoder() {
        if (ctx != nullptr) {
            avcodec_free_context(&ctx);
        }
    }
};

// Feed all due output packets to the muxer.
bool drainEncoder(AVFormatContext* mux, Encoder& enc, const char** errStage) {
    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(enc.ctx, pkt) == 0) {
        av_packet_rescale_ts(pkt, enc.ctx->time_base, enc.stream->time_base);
        pkt->stream_index = enc.stream->index;
        if (av_interleaved_write_frame(mux, pkt) < 0) {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            if (errStage != nullptr) {
                *errStage = "mux write";
            }
            return false;
        }
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    return true;
}

bool sendFrame(AVFormatContext* mux, Encoder& enc, AVFrame* frame /*nullable=flush*/) {
    if (avcodec_send_frame(enc.ctx, frame) < 0) {
        return false;
    }
    return drainEncoder(mux, enc, nullptr);
}

// Sequential per-asset reader: export walks time monotonically, so a cached
// decoder that only seeks backwards stays O(n). Non-monotonic requests
// (overlaps) re-seek — correct, just slower; S2 adds a frame cache.
struct ClipReader {
    std::string assetPath;
    media_ffmpeg::VideoDecoder decoder;
    core::Rational lastDelivered{-1, 1};
    bool ready = false;

    core::Result<media_ffmpeg::DecodedVideoFrame> at(const core::Rational& srcT,
                                                     const core::Rational& srcFps) {
        using R = core::Result<media_ffmpeg::DecodedVideoFrame>;
        if (!ready) {
            if (auto r = decoder.open(assetPath); r.isErr()) {
                return R::fail(r.error());
            }
            ready = true;
        }
        if (srcT < lastDelivered) { // rewind: seek and drop to target
            if (auto r = decoder.seek(srcT); r.isErr()) {
                return R::fail(r.error());
            }
        }
        const double frameDur =
            srcFps.num() > 0 ? 1.0 / static_cast<double>(srcFps) : 1.0 / 30.0;
        while (true) {
            auto f = decoder.nextFrame();
            if (f.isErr()) {
                if (f.error() == "eof" && !(srcT < lastDelivered)) {
                    // Target past EOF (rounding at tail): hold last frame.
                    return R::fail("eof");
                }
                return f;
            }
            const double pts = static_cast<double>(f.value().pts);
            if (pts + frameDur <= static_cast<double>(srcT) && frameDur > 0) {
                continue; // stale pre-target frame
            }
            lastDelivered = f.value().pts;
            return f;
        }
    }
};

} // namespace

core::Result<void> Mp4Exporter::run(const core::Project& project,
                                    const core::Id& sequenceId,
                                    const Mp4ExportOptions& options,
                                    const std::atomic_bool& cancel, ProgressFn progress) {
    using R = core::Result<void>;
    const auto report = [&](double p) {
        if (progress) {
            progress(p < 0 ? 0 : (p > 1 ? 1 : p));
        }
    };

    const core::Sequence* seq = nullptr;
    for (const auto& s : project.sequences) {
        if (s.id == sequenceId) {
            seq = &s;
            break;
        }
    }
    if (seq == nullptr) {
        return R::fail("sequence not found: " + sequenceId);
    }
    if (auto r = project.validate(); r.isErr()) {
        return R::fail("cannot export invalid project: " + r.error());
    }
    if (!(seq->fps.num() > 0)) {
        return R::fail("sequence fps must be positive");
    }
    const int canvasW = static_cast<int>(seq->width);
    const int canvasH = static_cast<int>(seq->height);
    if (canvasW <= 0 || canvasH <= 0) {
        return R::fail("sequence canvas must be positive");
    }
    if (!std::filesystem::exists(kFontFile)) {
        return R::fail("title font missing: " + std::string(kFontFile));
    }

    const core::Rational duration = render::Evaluator::sequenceDuration(*seq);
    if (duration.isZero() || duration.isNegative()) {
        return R::fail("nothing to export: sequence is empty");
    }
    const int64_t totalFrames = duration.toFramesRounded(seq->fps);
    if (totalFrames <= 0) {
        return R::fail("nothing to export: duration rounds to zero frames");
    }

    // ---- collect titles (text-kind tracks, enabled clips with content)
    struct TitleClip {
        core::Clip clip;
    };
    std::vector<TitleClip> titleClips;
    for (const auto& track : seq->tracks) {
        if (track.kind != core::TrackKind::Text) {
            continue;
        }
        for (const auto& clipId : track.clipIds) {
            const auto it = seq->clips.find(clipId);
            if (it == seq->clips.end() || !it->second.enabled || it->second.text.empty()) {
                continue;
            }
            titleClips.push_back(TitleClip{it->second});
        }
    }
    // Temp dir for title files (cleaned on every exit path below).
    const std::string tmpDir =
        (std::filesystem::temp_directory_path() / "native_editor_titles").string();
    std::error_code ec;
    std::filesystem::create_directories(tmpDir, ec);
    std::vector<TitleDef> titleDefs;
    std::vector<std::string> titleFiles;
    for (size_t i = 0; i < titleClips.size(); ++i) {
        const auto& c = titleClips[i].clip;
        auto tf = writeTitleFile(tmpDir, static_cast<int>(i), c.text);
        if (tf.isErr()) {
            return R::fail(tf.error());
        }
        titleFiles.push_back(tf.value());
        TitleDef d;
        d.file = tf.value();
        d.startSec = static_cast<double>(c.seqStart);
        d.endSec = static_cast<double>(c.seqEnd());
        d.fontPx = (std::max)(8, static_cast<int>(c.fontSizePt));
        d.dx = static_cast<int>(c.transform.x);
        d.dy = static_cast<int>(c.transform.y);
        titleDefs.push_back(std::move(d));
    }
    const auto cleanupTitles = [&] {
        for (const auto& f : titleFiles) {
            std::error_code e2;
            std::filesystem::remove(f, e2);
        }
    };

    // ---- video filter graph: buffersrc -> fit -> titles -> yuv420p -> sink
    // Built filter-by-filter (see makeFilter): titles carry Windows paths
    // and user text that must never pass through graph-string parsing.
    AVFilterGraph* graph = avfilter_graph_alloc();
    if (graph == nullptr) {
        cleanupTitles();
        return R::fail("cannot allocate filter graph");
    }
    auto failGraph = [&](const std::string& msg) -> R {
        avfilter_graph_free(&graph);
        cleanupTitles();
        return R::fail(msg);
    };

    const int64_t tbNum = seq->fps.den();
    const int64_t tbDen = seq->fps.num();
    char srcArgs[256];
    std::snprintf(srcArgs, sizeof(srcArgs), "video_size=%dx%d:pix_fmt=rgba:time_base=%lld/%lld:frame_rate=%lld/%lld",
                  canvasW, canvasH, static_cast<long long>(tbNum),
                  static_cast<long long>(tbDen), static_cast<long long>(seq->fps.num()),
                  static_cast<long long>(seq->fps.den()));
    std::string filterError;
    AVFilterContext* srcCtx = makeFilter(graph, "buffer", "in", srcArgs, nullptr, filterError);
    if (srcCtx == nullptr) {
        return failGraph(filterError);
    }
    AVFilterContext* sinkCtx =
        makeFilter(graph, "buffersink", "out", nullptr, nullptr, filterError);
    if (sinkCtx == nullptr) {
        return failGraph(filterError);
    }

    char fitArgs[256];
    std::snprintf(fitArgs, sizeof(fitArgs),
                  "%d:%d:force_original_aspect_ratio=decrease", canvasW, canvasH);
    AVFilterContext* scaleCtx = makeFilter(graph, "scale", "fit", fitArgs, nullptr, filterError);
    if (scaleCtx == nullptr) {
        return failGraph(filterError);
    }
    char padArgs[256];
    std::snprintf(padArgs, sizeof(padArgs), "%d:%d:(ow-iw)/2:(oh-ih)/2:color=black",
                  canvasW, canvasH);
    AVFilterContext* padCtx =
        makeFilter(graph, "pad", "letterbox", padArgs, nullptr, filterError);
    if (padCtx == nullptr) {
        return failGraph(filterError);
    }

    AVFilterContext* chainTail = padCtx;
    int titleIndex = 0;
    for (const auto& t : titleDefs) {
        char instance[32];
        std::snprintf(instance, sizeof(instance), "title%d", titleIndex++);
        const std::string x = "(w-text_w)/2" + std::string(t.dx >= 0 ? "+" : "") +
                              std::to_string(t.dx);
        const std::string y = "(h-text_h)/2" + std::string(t.dy >= 0 ? "+" : "") +
                              std::to_string(t.dy);
        const std::vector<std::pair<std::string, std::string>> textOpts = {
            {"fontfile", kFontFile},
            {"textfile", t.file},
            {"fontsize", std::to_string(t.fontPx)},
            {"fontcolor", "white"},
            {"x", x},
            {"y", y},
            {"enable",
             "between(t," + formatDouble(t.startSec) + "," + formatDouble(t.endSec) + ")"},
        };
        AVFilterContext* text =
            makeFilter(graph, "drawtext", instance, nullptr, &textOpts, filterError);
        if (text == nullptr) {
            return failGraph(filterError);
        }
        if (avfilter_link(chainTail, 0, text, 0) < 0) {
            return failGraph("cannot link title filter");
        }
        chainTail = text;
    }
    AVFilterContext* formatCtx =
        makeFilter(graph, "format", "to420", "yuv420p", nullptr, filterError);
    if (formatCtx == nullptr) {
        return failGraph(filterError);
    }
    if (avfilter_link(srcCtx, 0, scaleCtx, 0) < 0 ||
        avfilter_link(scaleCtx, 0, padCtx, 0) < 0 ||
        avfilter_link(chainTail, 0, formatCtx, 0) < 0 ||
        avfilter_link(formatCtx, 0, sinkCtx, 0) < 0) {
        return failGraph("cannot link video filter chain");
    }
    if (avfilter_graph_config(graph, nullptr) < 0) {
        return failGraph("cannot configure video filter chain");
    }

    // ---- muxer + encoders
    AVFormatContext* mux = nullptr;
    // FFmpeg 8.x allocator takes the filename too (format guessing fallback).
    if (avformat_alloc_output_context2(&mux, nullptr, "mp4", options.outPath.c_str()) < 0 ||
        mux == nullptr) {
        avfilter_graph_free(&graph);
        cleanupTitles();
        return R::fail("cannot allocate mp4 muxer");
    }
    auto failMux = [&](const std::string& msg) -> R {
        if (!(mux->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&mux->pb);
        }
        avformat_free_context(mux);
        avfilter_graph_free(&graph);
        cleanupTitles(); // idempotent: safe to run twice
        std::error_code e2;
        std::filesystem::remove(options.outPath, e2);
        return R::fail(msg);
    };

    Encoder videoEnc;
    {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (codec == nullptr) {
            return failMux("H.264 encoder not available in pinned FFmpeg");
        }
        videoEnc.ctx = avcodec_alloc_context3(codec);
        if (videoEnc.ctx == nullptr) {
            return failMux("cannot allocate H.264 encoder");
        }
        videoEnc.ctx->width = canvasW;
        videoEnc.ctx->height = canvasH;
        videoEnc.ctx->time_base = AVRational{static_cast<int>(tbNum), static_cast<int>(tbDen)};
        videoEnc.ctx->framerate = AVRational{static_cast<int>(seq->fps.num()),
                                             static_cast<int>(seq->fps.den())};
        videoEnc.ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        videoEnc.ctx->bit_rate = options.videoBitrateKbps * 1000;
        videoEnc.ctx->gop_size = 60;
        if (mux->oformat->flags & AVFMT_GLOBALHEADER) {
            videoEnc.ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        av_opt_set(videoEnc.ctx->priv_data, "preset", options.x264Preset.c_str(), 0);
        if (avcodec_open2(videoEnc.ctx, codec, nullptr) < 0) {
            return failMux("cannot open H.264 encoder");
        }
        videoEnc.stream = avformat_new_stream(mux, nullptr);
        if (videoEnc.stream == nullptr) {
            return failMux("cannot create video stream");
        }
        videoEnc.stream->time_base = videoEnc.ctx->time_base;
        if (avcodec_parameters_from_context(videoEnc.stream->codecpar, videoEnc.ctx) < 0) {
            return failMux("cannot copy video codec parameters");
        }
    }

    // Audio plan: ordered (start, clip) over the first audio track with clips.
    struct AudioSpan {
        core::Rational seqStart;
        core::Clip clip;
    };
    std::vector<AudioSpan> audioSpans;
    for (const auto& track : seq->tracks) {
        if (track.kind != core::TrackKind::Audio || track.muted) {
            continue;
        }
        for (const auto& clipId : track.clipIds) {
            const auto it = seq->clips.find(clipId);
            if (it == seq->clips.end() || !it->second.enabled || it->second.assetId.empty()) {
                continue;
            }
            audioSpans.push_back(AudioSpan{it->second.seqStart, it->second});
        }
        if (!audioSpans.empty()) {
            break; // S1: first audible audio track (mixing is Stage 2)
        }
    }
    std::sort(audioSpans.begin(), audioSpans.end(), [](const AudioSpan& a, const AudioSpan& b) {
        return a.seqStart < b.seqStart;
    });

    Encoder audioEnc;
    // Note: the AudioDecoder already normalizes to s16/stereo/48k, and the
    // exporter stages int samples as float/32768 — exact, so no SwrContext
    // is needed on this path (one conversion, not two).
    bool haveAudio = !audioSpans.empty();
    if (haveAudio) {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (codec == nullptr) {
            return failMux("AAC encoder not available in pinned FFmpeg");
        }
        audioEnc.ctx = avcodec_alloc_context3(codec);
        if (audioEnc.ctx == nullptr) {
            return failMux("cannot allocate AAC encoder");
        }
        audioEnc.ctx->sample_rate = media_ffmpeg::DecodedAudioChunk::kSampleRate;
        audioEnc.ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        av_channel_layout_default(&audioEnc.ctx->ch_layout,
                                  media_ffmpeg::DecodedAudioChunk::kChannels);
        audioEnc.ctx->bit_rate = options.audioBitrateKbps * 1000;
        audioEnc.ctx->time_base = AVRational{1, audioEnc.ctx->sample_rate};
        if (mux->oformat->flags & AVFMT_GLOBALHEADER) {
            audioEnc.ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        if (avcodec_open2(audioEnc.ctx, codec, nullptr) < 0) {
            return failMux("cannot open AAC encoder");
        }
        audioEnc.stream = avformat_new_stream(mux, nullptr);
        if (audioEnc.stream == nullptr) {
            return failMux("cannot create audio stream");
        }
        audioEnc.stream->time_base = audioEnc.ctx->time_base;
        if (avcodec_parameters_from_context(audioEnc.stream->codecpar, audioEnc.ctx) < 0) {
            return failMux("cannot copy audio codec parameters");
        }
    }

    AVDictionary* muxOpts = nullptr;
    av_dict_set(&muxOpts, "movflags", "+faststart", 0);
    if (!(mux->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&mux->pb, options.outPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            av_dict_free(&muxOpts);
            return failMux("cannot open output file: " + options.outPath);
        }
    }
    if (avformat_write_header(mux, &muxOpts) < 0) {
        av_dict_free(&muxOpts);
        return failMux("cannot write mp4 header");
    }
    av_dict_free(&muxOpts);

    // ---- video: evaluator -> decode -> filter -> encode
    std::map<core::Id, ClipReader> readers;
    auto assetPath = [&](const core::Id& id) -> const std::string& {
        return project.assets.at(id).path;
    };
    AVFrame* filtFrame = av_frame_alloc();
    bool videoFailed = false;
    std::string videoError;
    for (int64_t i = 0; i < totalFrames && !videoFailed; ++i) {
        if (cancel.load()) {
            videoError = "cancelled";
            videoFailed = true;
            break;
        }
        const core::Rational t = core::Rational::fromFrames(i, seq->fps);
        const render::FramePlan plan = render::Evaluator::evaluateVideoAt(*seq, t);
        // Topmost video layer wins in S1 (documented; blend arrives in S2).
        const render::PlacedClip* videoLayer = nullptr;
        for (const auto& layer : plan.layers) {
            if (layer.trackKind == core::TrackKind::Video && !layer.assetId.empty()) {
                videoLayer = &layer;
            }
        }
        media_ffmpeg::DecodedVideoFrame decoded;
        bool havePicture = false;
        if (videoLayer != nullptr) {
            ClipReader& reader = readers[videoLayer->assetId];
            if (!reader.ready) {
                reader.assetPath = assetPath(videoLayer->assetId);
            }
            const auto& asset = project.assets.at(videoLayer->assetId);
            auto got = reader.at(videoLayer->sourceTime, asset.fps);
            if (got.isOk()) {
                decoded = std::move(got.value());
                havePicture = true;
            } else if (got.error() != "eof") {
                videoError = "decode failed at frame " + std::to_string(i) + ": " + got.error();
                videoFailed = true;
                break;
            }
            // "eof" at the tail (rounding): hold black; never abort the file.
        }
        // Push RGBA (or black) into the graph; titles composite inside.
        AVFrame* src = av_frame_alloc();
        src->format = AV_PIX_FMT_RGBA;
        src->width = canvasW;
        src->height = canvasH;
        src->pts = i;
        if (av_frame_get_buffer(src, 0) < 0) {
            av_frame_free(&src);
            videoError = "cannot allocate export frame";
            videoFailed = true;
            break;
        }
        // Start from black every frame (letterbox/pillarbox regions and
        // pictureless gaps must be defined pixels, never stale memory).
        for (int y = 0; y < canvasH; ++y) {
            std::memset(src->data[0] + y * src->linesize[0], 0,
                        static_cast<size_t>(canvasW) * 4);
        }
        if (havePicture && decoded.width > 0 && decoded.height > 0) {
            // Center-crop/pad the decoded picture into the canvas buffer
            // (the scale filter also fits, but feeding exact canvas keeps
            // the graph's live config stable).
            const int copyW = (std::min)(decoded.width, canvasW);
            const int copyH = (std::min)(decoded.height, canvasH);
            const int dstX = (canvasW - copyW) / 2;
            const int dstY = (canvasH - copyH) / 2;
            const int srcX = (decoded.width - copyW) / 2;
            const int srcY = (decoded.height - copyH) / 2;
            for (int y = 0; y < copyH; ++y) {
                std::memcpy(src->data[0] + (dstY + y) * src->linesize[0] + dstX * 4,
                            decoded.rgba.data() +
                                static_cast<size_t>(srcY + y) * decoded.width * 4 + srcX * 4,
                            static_cast<size_t>(copyW) * 4);
            }
        }
        if (av_buffersrc_add_frame_flags(srcCtx, src, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
            av_frame_free(&src);
            videoError = "cannot feed video filter graph";
            videoFailed = true;
            break;
        }
        av_frame_free(&src);
        while (av_buffersink_get_frame(sinkCtx, filtFrame) == 0) {
            filtFrame->pts = videoEnc.nextPts++;
            if (!sendFrame(mux, videoEnc, filtFrame)) {
                videoError = "video encode failed at frame " + std::to_string(i);
                videoFailed = true;
            }
            av_frame_unref(filtFrame);
            if (videoFailed) {
                break;
            }
        }
        report(0.9 * static_cast<double>(i + 1) / static_cast<double>(totalFrames));
    }
    av_frame_free(&filtFrame);
    if (!videoFailed) {
        av_buffersrc_close(srcCtx, AV_NOPTS_VALUE, 0);
        AVFrame* tail = av_frame_alloc();
        while (av_buffersink_get_frame(sinkCtx, tail) == 0) {
            tail->pts = videoEnc.nextPts++;
            if (!sendFrame(mux, videoEnc, tail)) {
                videoError = "video encode failed while draining";
                videoFailed = true;
            }
            av_frame_unref(tail);
            if (videoFailed) {
                break;
            }
        }
        av_frame_free(&tail);
        if (!videoFailed && !sendFrame(mux, videoEnc, nullptr)) {
            videoError = "video encode failed while flushing";
            videoFailed = true;
        }
        if (!videoFailed) {
            const char* stage = nullptr;
            if (!drainEncoder(mux, videoEnc, &stage)) {
                videoError = std::string("video mux failed (") + (stage ? stage : "?") + ")";
                videoFailed = true;
            }
        }
    }
    if (videoFailed) {
        avfilter_graph_free(&graph);
        auto r = failMux(videoError.empty() ? "video export failed" : videoError);
        cleanupTitles();
        if (videoError == "cancelled") {
            return R::fail("cancelled");
        }
        return r;
    }

    // ---- audio: spans in order, silence in gaps, sequential decode
    if (haveAudio) {
        const int frameSize =
            audioEnc.ctx->frame_size > 0 ? audioEnc.ctx->frame_size : 1024;
        AVFrame* af = av_frame_alloc();
        std::vector<float> fltp(static_cast<size_t>(frameSize) *
                                media_ffmpeg::DecodedAudioChunk::kChannels);
        size_t fltpUsed = 0; // samples per channel buffered
        bool audioFailed = false;
        std::string audioError;

        auto flushAudioFrame = [&](bool pad) -> bool {
            if (!pad && fltpUsed != static_cast<size_t>(frameSize)) {
                return true;
            }
            // av_frame_unref() resets fields, so re-establish them per flush.
            af->format = AV_SAMPLE_FMT_FLTP;
            av_channel_layout_copy(&af->ch_layout, &audioEnc.ctx->ch_layout);
            af->sample_rate = audioEnc.ctx->sample_rate;
            af->nb_samples = frameSize;
            if (av_frame_get_buffer(af, 0) < 0) {
                return false;
            }
            // Staged int samples -> planar float.
            for (int c = 0; c < media_ffmpeg::DecodedAudioChunk::kChannels; ++c) {
                float* plane = reinterpret_cast<float*>(af->data[c]);
                for (int s = 0; s < frameSize; ++s) {
                    plane[s] = static_cast<size_t>(s) < fltpUsed
                                   ? fltp[static_cast<size_t>(s) * 2 + static_cast<size_t>(c)] /
                                         32768.0f
                                   : 0.0f;
                }
            }
            af->pts = audioEnc.nextPts;
            audioEnc.nextPts += frameSize;
            fltpUsed = 0;
            if (!sendFrame(mux, audioEnc, af)) {
                return false;
            }
            av_frame_unref(af);
            return true;
        };

        auto feedS16 = [&](const int16_t* pcm, size_t samplesPerChannel) -> bool {
            size_t pos = 0;
            while (pos < samplesPerChannel) {
                const size_t room = static_cast<size_t>(frameSize) - fltpUsed;
                const size_t take = (std::min)(room, samplesPerChannel - pos);
                for (size_t s = 0; s < take; ++s) {
                    for (int c = 0; c < media_ffmpeg::DecodedAudioChunk::kChannels; ++c) {
                        fltp[(fltpUsed + s) * 2 + static_cast<size_t>(c)] =
                            static_cast<float>(
                                pcm[(pos + s) * 2 + static_cast<size_t>(c)]);
                    }
                }
                fltpUsed += take;
                pos += take;
                if (fltpUsed == static_cast<size_t>(frameSize)) {
                    if (!flushAudioFrame(false)) {
                        return false;
                    }
                }
            }
            return true;
        };

        core::Rational audioCursor{0};
        for (const auto& span : audioSpans) {
            if (cancel.load() || audioFailed) {
                break;
            }
            // Silence gap before this span.
            if (audioCursor < span.seqStart) {
                const int64_t gapSamples =
                    (span.seqStart - audioCursor)
                        .toFramesRounded(core::Rational(48000, 1));
                std::vector<int16_t> silence(static_cast<size_t>(gapSamples) * 2, 0);
                if (!feedS16(silence.data(), static_cast<size_t>(gapSamples))) {
                    audioError = "audio encode failed in gap";
                    audioFailed = true;
                    break;
                }
                audioCursor = span.seqStart;
            }
            const auto& asset = project.assets.at(span.clip.assetId);
            media_ffmpeg::AudioDecoder decoder;
            if (auto r = decoder.open(asset.path); r.isErr()) {
                audioError = "cannot open audio: " + r.error();
                audioFailed = true;
                break;
            }
            if (!decoder.hasAudio()) {
                audioCursor = span.clip.seqEnd(); // treat as silence already fed? no—
                // video-only asset on an audio track: advance cursor, no samples.
                continue;
            }
            if (auto r = decoder.seek(span.clip.sourceIn); r.isErr()) {
                audioError = "audio seek failed: " + r.error();
                audioFailed = true;
                break;
            }
            int64_t need =
                span.clip.seqDuration().toFramesRounded(core::Rational(48000, 1));
            while (need > 0) {
                if (cancel.load()) {
                    break;
                }
                auto chunk =
                    decoder.nextChunk(static_cast<int>((std::min<int64_t>)(need, 4096)));
                if (chunk.isErr()) {
                    if (chunk.error() == "eof") {
                        break; // short source: pad with silence below
                    }
                    audioError = "audio decode failed: " + chunk.error();
                    audioFailed = true;
                    break;
                }
                const size_t got =
                    chunk.value().pcm.size() / media_ffmpeg::DecodedAudioChunk::kChannels;
                if (got == 0) {
                    break;
                }
                const size_t use = static_cast<size_t>((std::min<int64_t>)(need, static_cast<int64_t>(got)));
                if (!feedS16(chunk.value().pcm.data(), use)) {
                    audioError = "audio encode failed";
                    audioFailed = true;
                    break;
                }
                need -= static_cast<int64_t>(use);
                if (use < got) {
                    break; // consumed enough
                }
            }
            if (audioFailed) {
                break;
            }
            if (need > 0) { // short source: pad tail with silence
                std::vector<int16_t> silence(static_cast<size_t>(need) * 2, 0);
                if (!feedS16(silence.data(), static_cast<size_t>(need))) {
                    audioError = "audio encode failed in tail pad";
                    audioFailed = true;
                    break;
                }
            }
            audioCursor = span.clip.seqEnd();
            // Display-only double conversion for progress (never persisted).
            const double doneSec = static_cast<double>(audioCursor);
            const double totalSec = static_cast<double>(duration);
            report(0.9 + (totalSec > 0 ? 0.1 * doneSec / totalSec : 0.1));
        }
        // Tail silence to sequence duration.
        if (!audioFailed && !cancel.load() && audioCursor < duration) {
            const int64_t tail =
                (duration - audioCursor).toFramesRounded(core::Rational(48000, 1));
            std::vector<int16_t> silence(static_cast<size_t>(tail) * 2, 0);
            if (!feedS16(silence.data(), static_cast<size_t>(tail))) {
                audioError = "audio encode failed in tail";
                audioFailed = true;
            }
        }
        if (fltpUsed > 0 && !audioFailed && !cancel.load()) {
            if (!flushAudioFrame(true)) {
                audioError = "audio encode failed flushing";
                audioFailed = true;
            }
        }
        if (!audioFailed && !cancel.load() && !sendFrame(mux, audioEnc, nullptr)) {
            audioError = "audio encode failed while flushing";
            audioFailed = true;
        }
        if (!audioFailed && !cancel.load()) {
            const char* stage = nullptr;
            if (!drainEncoder(mux, audioEnc, &stage)) {
                audioError = "audio mux failed";
                audioFailed = true;
            }
        }
        av_frame_free(&af);
        if (cancel.load()) {
            avfilter_graph_free(&graph);
            auto r = failMux("cancelled");
            cleanupTitles();
            return R::fail("cancelled");
        }
        if (audioFailed) {
            avfilter_graph_free(&graph);
            auto r = failMux(audioError.empty() ? "audio export failed" : audioError);
            cleanupTitles();
            return r;
        }
    }

    if (av_write_trailer(mux) < 0) {
        avfilter_graph_free(&graph);
        auto r = failMux("cannot write mp4 trailer");
        cleanupTitles();
        return r;
    }
    if (!(mux->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&mux->pb);
    }
    avformat_free_context(mux);
    avfilter_graph_free(&graph);
    cleanupTitles();
    report(1.0);
    return R::ok();
}

} // namespace editor::export_ffmpeg
