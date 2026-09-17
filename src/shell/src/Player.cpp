#include "shell/Player.hpp"
#include "shell/FrameCompositor.hpp"
#include "shell/Session.hpp"

#include "media_ffmpeg/FrameDecoder.hpp"
#include "render/Evaluator.hpp"

#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioSink>
#include <QtMultimedia/QMediaDevices>
#include <QTimer>

#include <algorithm>
#include <vector>

namespace editor::shell {

struct Player::AudioPump final : public QThread {
    Q_OBJECT
public: // (Q_OBJECT re-opens a private: section — keep members public)
    core::Project project;
    QString sequenceId;
    double fromSec = 0.0;
    std::atomic<unsigned long long>* generation = nullptr;
    unsigned long long myGeneration = 0;
    std::atomic_bool* pausedFlag = nullptr;
    std::atomic<float>* volumeParam = nullptr;
    std::atomic_bool* mutedParam = nullptr;

    void run() override {
        const core::Sequence* seq = nullptr;
        for (const auto& s : project.sequences) {
            if (s.id == sequenceId.toStdString()) {
                seq = &s;
                break;
            }
        }
        if (seq == nullptr) {
            emit error("Playback sequence vanished");
            emit finished(false);
            return;
        }
        // Span walk identical in meaning to the exporter: ordered audio
        // clips, digital silence in gaps. (S2: share one AudioTimeline.)
        struct Span {
            core::Rational start;
            core::Clip clip;
        };
        std::vector<Span> spans;
        for (const auto& track : seq->tracks) {
            if (track.kind != core::TrackKind::Audio || track.muted) {
                continue;
            }
            for (const auto& id : track.clipIds) {
                const auto it = seq->clips.find(id);
                if (it == seq->clips.end() || !it->second.enabled ||
                    it->second.assetId.empty()) {
                    continue;
                }
                spans.push_back(Span{it->second.seqStart, it->second});
            }
            if (!spans.empty()) {
                break;
            }
        }
        // Fallback: if no audio track has clips, check unmuted video tracks for media with audio
        if (spans.empty()) {
            for (const auto& track : seq->tracks) {
                if (track.kind != core::TrackKind::Video || track.muted) {
                    continue;
                }
                for (const auto& id : track.clipIds) {
                    const auto it = seq->clips.find(id);
                    if (it == seq->clips.end() || !it->second.enabled ||
                        it->second.assetId.empty()) {
                        continue;
                    }
                    const auto ait = project.assets.find(it->second.assetId);
                    if (ait != project.assets.end() && ait->second.hasAudio) {
                        spans.push_back(Span{it->second.seqStart, it->second});
                    }
                }
                if (!spans.empty()) {
                    break;
                }
            }
        }
        std::sort(spans.begin(), spans.end(),
                  [](const Span& a, const Span& b) { return a.start < b.start; });
        const core::Rational end = render::Evaluator::sequenceDuration(*seq);

        QAudioFormat format;
        format.setSampleRate(media_ffmpeg::DecodedAudioChunk::kSampleRate);
        format.setChannelCount(media_ffmpeg::DecodedAudioChunk::kChannels);
        format.setSampleFormat(QAudioFormat::Int16);
        QAudioSink* sink = nullptr;
        QIODevice* out = nullptr;
        bool useFloat = false;
        const QAudioDevice device = QMediaDevices::defaultAudioOutput();
        if (!device.isNull()) {
            if (!device.isFormatSupported(format)) {
                format.setSampleFormat(QAudioFormat::Float);
            }
            if (device.isFormatSupported(format)) {
                useFloat = (format.sampleFormat() == QAudioFormat::Float);
                sink = new QAudioSink(device, format);
                const int sampleSize = useFloat ? sizeof(float) : sizeof(std::int16_t);
                sink->setBufferSize(4 * 4096 * 2 * sampleSize);
                out = sink->start();
                if (out == nullptr) {
                    delete sink;
                    sink = nullptr;
                }
            }
        }
        // No device (headless/CI): decode-and-discard paced to realtime so
        // video timing stays honest. Reported once via error() as info.
        bool reportedNoAudio = false;

        const auto alive = [&] {
            return generation != nullptr && generation->load() == myGeneration;
        };
        int64_t writtenSamples = 0; // per channel, incl. silence
        const double runStart = fromSec;
        double lastTick = -1.0;

        const auto pump = [&](const std::int16_t* pcm, int64_t samples, float clipGain = 1.0f) -> bool {
            int64_t pos = 0;
            const int sampleSize = useFloat ? sizeof(float) : sizeof(std::int16_t);
            const int bytesPerFrame = 2 * sampleSize;

            while (pos < samples && alive()) {
                while (pausedFlag != nullptr && pausedFlag->load() && alive()) {
                    if (sink != nullptr) {
                        sink->suspend();
                    }
                    QThread::msleep(10);
                }
                if (!alive()) {
                    return false;
                }
                if (sink != nullptr && out != nullptr) {
                    sink->resume();
                    const int64_t bytesFree = sink->bytesFree();
                    const int64_t want =
                        (std::min<int64_t>)(samples - pos, bytesFree / bytesPerFrame);
                    if (want <= 0) {
                        QThread::msleep(3);
                        continue;
                    }

                    const float masterVol = (mutedParam != nullptr && mutedParam->load())
                                                ? 0.0f
                                                : (volumeParam != nullptr ? volumeParam->load() : 1.0f);
                    const float effGain = masterVol * clipGain;

                    qint64 wroteBytes = 0;
                    if (useFloat) {
                        std::vector<float> floatBuf(static_cast<size_t>(want) * 2);
                        for (size_t i = 0; i < static_cast<size_t>(want) * 2; ++i) {
                            float s = (static_cast<float>(pcm[pos * 2 + i]) / 32768.0f) * effGain;
                            floatBuf[i] = std::clamp(s, -1.0f, 1.0f);
                        }
                        wroteBytes = out->write(reinterpret_cast<const char*>(floatBuf.data()),
                                                static_cast<qint64>(want * bytesPerFrame));
                    } else {
                        std::vector<std::int16_t> intBuf(static_cast<size_t>(want) * 2);
                        for (size_t i = 0; i < static_cast<size_t>(want) * 2; ++i) {
                            float s = static_cast<float>(pcm[pos * 2 + i]) * effGain;
                            intBuf[i] = static_cast<std::int16_t>(std::clamp(s, -32768.0f, 32767.0f));
                        }
                        wroteBytes = out->write(reinterpret_cast<const char*>(intBuf.data()),
                                                static_cast<qint64>(want * bytesPerFrame));
                    }

                    if (wroteBytes <= 0) {
                        QThread::msleep(3);
                        continue;
                    }
                    const int64_t wroteFrames = wroteBytes / bytesPerFrame;
                    pos += wroteFrames;
                    writtenSamples += wroteFrames;
                } else {
                    // Timer fallback: pace by chunk duration.
                    if (!reportedNoAudio) {
                        reportedNoAudio = true;
                        emit error("No audio output device available — video-only clock");
                    }
                    const int64_t step = (std::min<int64_t>)(samples - pos, 2048);
                    QThread::msleep(static_cast<unsigned long>(
                        step * 1000 / media_ffmpeg::DecodedAudioChunk::kSampleRate));
                    pos += step;
                    writtenSamples += step;
                }
                const double now = sink != nullptr
                                       ? runStart + sink->processedUSecs() / 1000000.0
                                       : runStart + writtenSamples / 48000.0;
                if (now - lastTick >= 0.05) {
                    lastTick = now;
                    emit tick(now);
                }
            }
            return alive();
        };

        core::Rational cursor = rationalFromSeconds(fromSec);
        bool ok = true;
        for (const auto& span : spans) {
            if (!alive()) {
                ok = false;
                break;
            }
            if (cursor < rationalFromSeconds(fromSec)) {
                cursor = rationalFromSeconds(fromSec);
            }
            if (span.clip.seqEnd() <= cursor) {
                continue;
            }
            const core::Rational playFrom =
                cursor < span.start ? span.start : cursor; // max
            if (cursor < span.start) {
                const int64_t gap =
                    (span.start - cursor).toFramesRounded(core::Rational(48000, 1));
                std::vector<std::int16_t> silence(static_cast<size_t>(gap) * 2, 0);
                if (!pump(silence.data(), gap, 0.0f)) {
                    ok = false;
                    break;
                }
                cursor = span.start;
            }
            const auto ait = project.assets.find(span.clip.assetId);
            if (ait == project.assets.end()) {
                ok = false;
                emit error("Playback asset vanished");
                break;
            }
            media_ffmpeg::AudioDecoder decoder;
            if (decoder.open(ait->second.path).isErr() || !decoder.hasAudio()) {
                cursor = span.clip.seqEnd();
                continue;
            }
            const core::Rational srcSeek = span.clip.mapToSource(playFrom);
            if (decoder.seek(srcSeek).isErr()) {
                cursor = span.clip.seqEnd();
                continue;
            }
            int64_t need =
                (span.clip.seqEnd() - playFrom).toFramesRounded(core::Rational(48000, 1));
            const float clipGain = static_cast<float>(span.clip.opacity);
            const double speed = span.clip.speed.num() > 0 ? static_cast<double>(span.clip.speed) : 1.0;

            if (std::abs(speed - 1.0) < 1e-4) {
                while (need > 0 && alive()) {
                    auto chunk = decoder.nextChunk(static_cast<int>((std::min<int64_t>)(need, 4096)));
                    if (chunk.isErr()) {
                        break; // eof or drain: pad below
                    }
                    const size_t got =
                        chunk.value().pcm.size() / media_ffmpeg::DecodedAudioChunk::kChannels;
                    if (got == 0) {
                        break;
                    }
                    const auto use =
                        static_cast<size_t>((std::min<int64_t>)(need, static_cast<int64_t>(got)));
                    if (!pump(chunk.value().pcm.data(), static_cast<int64_t>(use), clipGain)) {
                        ok = false;
                        break;
                    }
                    need -= static_cast<int64_t>(use);
                    if (use < got) {
                        break;
                    }
                }
            } else {
                std::vector<std::int16_t> srcBuf;
                double srcPos = 0.0;
                while (need > 0 && alive()) {
                    const int64_t block = (std::min<int64_t>)(need, 2048);
                    const double maxSrcPos = srcPos + static_cast<double>(block) * speed;
                    const size_t neededSrcFrames = static_cast<size_t>(std::ceil(maxSrcPos)) + 2;

                    while (srcBuf.size() / 2 < neededSrcFrames && alive()) {
                        auto chunk = decoder.nextChunk(4096);
                        if (chunk.isErr() || chunk.value().pcm.empty()) {
                            break;
                        }
                        srcBuf.insert(srcBuf.end(), chunk.value().pcm.begin(), chunk.value().pcm.end());
                    }

                    const size_t srcFrames = srcBuf.size() / 2;
                    if (srcFrames == 0) {
                        break;
                    }

                    std::vector<std::int16_t> outBlock(static_cast<size_t>(block) * 2, 0);
                    size_t generated = 0;
                    for (int64_t i = 0; i < block; ++i) {
                        const double pos = srcPos + static_cast<double>(i) * speed;
                        const size_t idx0 = static_cast<size_t>(pos);
                        if (idx0 >= srcFrames) {
                            break;
                        }
                        const size_t idx1 = (idx0 + 1 < srcFrames) ? (idx0 + 1) : idx0;
                        const double frac = pos - static_cast<double>(idx0);
                        for (int c = 0; c < 2; ++c) {
                            const double s0 = static_cast<double>(srcBuf[idx0 * 2 + c]);
                            const double s1 = static_cast<double>(srcBuf[idx1 * 2 + c]);
                            const double interp = s0 + frac * (s1 - s0);
                            outBlock[static_cast<size_t>(i) * 2 + c] = static_cast<std::int16_t>(
                                std::clamp(interp, -32768.0, 32767.0));
                        }
                        generated++;
                    }

                    if (generated == 0) {
                        break;
                    }

                    if (!pump(outBlock.data(), static_cast<int64_t>(generated), clipGain)) {
                        ok = false;
                        break;
                    }

                    srcPos += static_cast<double>(generated) * speed;
                    need -= static_cast<int64_t>(generated);

                    const size_t drop = static_cast<size_t>(srcPos);
                    if (drop > 0 && drop <= srcBuf.size() / 2) {
                        srcBuf.erase(srcBuf.begin(), srcBuf.begin() + drop * 2);
                        srcPos -= static_cast<double>(drop);
                    }

                    if (generated < static_cast<size_t>(block)) {
                        break;
                    }
                }
            }
            if (!ok) {
                break;
            }
            if (need > 0) {
                std::vector<std::int16_t> silence(static_cast<size_t>(need) * 2, 0);
                if (!pump(silence.data(), need, 0.0f)) {
                    ok = false;
                    break;
                }
            }
            cursor = span.clip.seqEnd();
        }
        // Tail silence to sequence end so the clock reaches duration.
        if (ok && alive() && cursor < end) {
            const int64_t tail = (end - cursor).toFramesRounded(core::Rational(48000, 1));
            std::vector<std::int16_t> silence(static_cast<size_t>(tail) * 2, 0);
            ok = pump(silence.data(), tail, 0.0f);
        }
        if (sink != nullptr) {
            // Let the buffered tail play out briefly, then stop.
            int guard = 0;
            while (alive() && sink->state() == QAudio::ActiveState && guard++ < 400) {
                QThread::msleep(10);
            }
            sink->stop();
            delete sink;
            sink = nullptr;
            out = nullptr;
        }
        emit finished(ok && alive());
    }

signals:
    void tick(double seconds);
    void finished(bool naturalEnd);
    void error(const QString& message);
};

struct Player::VideoWorker final : public QObject {
    Q_OBJECT
public:
    FrameCompositor compositor;

public slots:
    void requestFrame(core::Project project, QString sequenceId, double seconds,
                      unsigned long long gen) {
        const core::Sequence* seq = nullptr;
        for (const auto& s : project.sequences) {
            if (s.id == sequenceId.toStdString()) {
                seq = &s;
                break;
            }
        }
        if (seq == nullptr) {
            return;
        }
        const QImage img =
            compositor.frameAt(project, *seq, rationalFromSeconds(seconds));
        emit ready(img, seconds, gen);
    }

signals:
    void ready(const QImage& image, double seconds, unsigned long long gen);
};

Player::Player(QObject* parent) : QObject(parent) {
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    audioOutputAvailable_ = !dev.isNull();
    videoTimer_ = new QTimer(this);
    videoTimer_->setInterval(33);
    connect(videoTimer_, &QTimer::timeout, this, &Player::onVideoTickTimeout);
}

void Player::setVolume(double v) {
    v = std::clamp(v, 0.0, 1.0);
    if (std::abs(volume_ - v) > 1e-4) {
        volume_ = v;
        volumeAtomic_.store(static_cast<float>(v));
        emit volumeChanged();
    }
}

void Player::setMuted(bool m) {
    if (muted_ != m) {
        muted_ = m;
        mutedAtomic_.store(m);
        emit mutedChanged();
    }
}

Player::~Player() {
    generation_.fetch_add(1);
    stopPump();
    if (videoThread_ != nullptr) {
        videoThread_->quit();
        videoThread_->wait();
    }
}

void Player::setSession(Session* session) {
    session_ = session;
    if (session_ != nullptr) {
        connect(session_, &Session::projectChanged, this, &Player::refreshDuration);
        if (videoWorker_ == nullptr) {
            videoThread_ = new QThread(this);
            videoWorker_ = new VideoWorker();
            videoWorker_->moveToThread(videoThread_);
            connect(videoWorker_, &VideoWorker::ready, this, &Player::onVideoReady);
            videoThread_->start();
        }
    }
    refreshDuration();
}

void Player::play() {
    if (session_ == nullptr || playing_) {
        return;
    }
    refreshDuration();
    double from = positionSec_;
    if (from < 0 || (durationSec_ > 0 && from >= durationSec_ - 0.05)) {
        from = 0;
    }
    positionSec_ = from;
    emit positionChanged();
    startPump(from);
    playing_ = true;
    emit playingChanged();
    videoTimer_->start();
}

void Player::pause() {
    if (!playing_) {
        return;
    }
    if (audioPump_ != nullptr && audioPump_->pausedFlag != nullptr) {
        audioPump_->pausedFlag->store(true);
    }
    playing_ = false;
    emit playingChanged();
    videoTimer_->stop();
}

void Player::toggle() {
    if (playing_) {
        pause();
    } else {
        if (durationSec_ > 0 && positionSec_ >= durationSec_ - 0.05) {
            seekTo(0);
        }
        if (audioPump_ != nullptr && audioPump_->isRunning()) {
            // Resume a paused pump instead of restarting the clock.
            audioPump_->pausedFlag->store(false);
            playing_ = true;
            emit playingChanged();
            videoTimer_->start();
        } else {
            play();
        }
    }
}

void Player::stop() {
    stopPump();
    playing_ = false;
    emit playingChanged();
    videoTimer_->stop();
    positionSec_ = 0.0;
    emit positionChanged();
}

void Player::seekTo(double seconds) {
    if (seconds < 0) {
        seconds = 0;
    }
    if (durationSec_ > 0 && seconds > durationSec_) {
        seconds = durationSec_;
    }
    const bool wasPlaying = playing_;
    stopPump();
    playing_ = false;
    emit playingChanged();
    videoTimer_->stop();
    positionSec_ = seconds;
    emit positionChanged();
    if (wasPlaying) {
        startPump(seconds);
        playing_ = true;
        emit playingChanged();
        videoTimer_->start();
    }
}

void Player::startPump(double fromSec) {
    stopPump();
    if (session_ == nullptr) {
        return;
    }
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    const bool available = !dev.isNull();
    if (audioOutputAvailable_ != available) {
        audioOutputAvailable_ = available;
        emit audioOutputAvailableChanged();
    }
    const unsigned long long gen = generation_.fetch_add(1) + 1;
    // Note: the pump IS the thread (QThread subclass with overridden run(),
    // no event loop). It is joined and deleted in stopPump(); never
    // deleteLater, which would never be delivered without an event loop.
    audioPump_ = new AudioPump();
    audioPump_->project = session_->project();
    audioPump_->sequenceId = QString::fromStdString(
        session_->project().activeSequence() != nullptr
            ? session_->project().activeSequence()->id
            : std::string());
    audioPump_->fromSec = fromSec;
    audioPump_->generation = &generation_;
    audioPump_->myGeneration = gen;
    audioPump_->pausedFlag = new std::atomic_bool(false);
    audioPump_->volumeParam = &volumeAtomic_;
    audioPump_->mutedParam = &mutedAtomic_;
    connect(audioPump_, &AudioPump::tick, this, &Player::onAudioTick);
    connect(audioPump_, &AudioPump::finished, this, &Player::onAudioFinished);
    connect(audioPump_, &AudioPump::error, this, &Player::error);
    audioPump_->start();
}

void Player::stopPump() {
    generation_.fetch_add(1);
    if (audioPump_ != nullptr) {
        // Decode steps are short, so the join is quick; terminate() is a
        // last-resort guard that should never fire (and is reported if it
        // does, since a killed decoder thread may leak its FFmpeg context).
        audioPump_->wait(5000);
        if (audioPump_->isRunning()) {
            audioPump_->terminate();
            audioPump_->wait(2000);
            emit error("Playback thread had to be force-stopped");
        }
        delete audioPump_->pausedFlag;
        delete audioPump_;
        audioPump_ = nullptr;
    }
    framePending_ = false;
}

void Player::refreshDuration() {
    double d = 0.0;
    if (session_ != nullptr) {
        if (const core::Sequence* seq = session_->project().activeSequence()) {
            d = static_cast<double>(render::Evaluator::sequenceDuration(*seq));
        }
    }
    if (durationSec_ != d) {
        durationSec_ = d;
        emit durationChanged();
    }
    if (positionSec_ > durationSec_) {
        positionSec_ = durationSec_;
        emit positionChanged();
    }
}

void Player::onAudioTick(double seconds) {
    positionSec_ = seconds;
    emit positionChanged();
}

void Player::onAudioFinished(bool naturalEnd) {
    if (!naturalEnd) {
        return; // superseded by a newer generation (seek/stop)
    }
    stopPump();
    playing_ = false;
    emit playingChanged();
    videoTimer_->stop();
    emit playbackFinished();
}

void Player::onVideoReady(const QImage& image, double seconds, unsigned long long gen) {
    framePending_ = false;
    if (!playing_ || gen != generation_.load()) {
        return; // stale frame from a previous run
    }
    emit videoFrame(image, seconds);
}

void Player::onVideoTickTimeout() {
    if (!playing_ || framePending_ || session_ == nullptr || videoWorker_ == nullptr) {
        return;
    }
    const core::Sequence* seq = session_->project().activeSequence();
    if (seq == nullptr) {
        return;
    }
    framePending_ = true;
    const core::Project snapshot = session_->project();
    const QString seqId = QString::fromStdString(seq->id);
    const double at = positionSec_;
    const unsigned long long gen = generation_.load();
    QMetaObject::invokeMethod(
        videoWorker_, [this, snapshot, seqId, at, gen] {
            videoWorker_->requestFrame(snapshot, seqId, at, gen);
        });
}

} // namespace editor::shell

#include "Player.moc"
