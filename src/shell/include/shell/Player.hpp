#pragma once

// Audio-clocked player (BUILD-PLAN media rule: audio is the playback clock,
// video is evaluated against it).
//
// Two workers, both off the UI thread:
//   AudioPump thread — sequential span walk (clips + digital silence, same
//     semantics as the exporter), push-mode QAudioSink. Position comes from
//     QAudioSink::processedUSecs (heard audio), not from samples written.
//   VideoWorker thread — owns a FrameCompositor, decodes only the latest
//     requested time (framePending handshake drops stale requests).
// No audio device -> timer fallback clock (video-only), reported via error().

#include "core/Model.hpp"

#include <QImage>
#include <QObject>
#include <QThread>

#include <atomic>
#include <memory>

class QTimer;

namespace editor::shell {

class Session;

class Player final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(double positionSec READ positionSec NOTIFY positionChanged)
    Q_PROPERTY(double durationSec READ durationSec NOTIFY durationChanged)

public:
    explicit Player(QObject* parent = nullptr);
    ~Player() override;

    void setSession(Session* session);

    bool playing() const { return playing_; }
    double positionSec() const { return positionSec_; }
    double durationSec() const { return durationSec_; }

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seekTo(double seconds);

signals:
    void playingChanged();
    void positionChanged();
    void durationChanged();
    void playbackFinished();
    void error(const QString& message);
    void videoFrame(const QImage& image, double seconds);

private slots:
    void onAudioTick(double seconds);
    void onAudioFinished(bool naturalEnd);
    void onVideoReady(const QImage& image, double seconds, unsigned long long gen);
    void onVideoTickTimeout();

private:
    struct AudioPump;
    struct VideoWorker;

    void startPump(double fromSec);
    void stopPump();
    void refreshDuration();

    Session* session_ = nullptr;
    bool playing_ = false;
    double positionSec_ = 0.0;
    double durationSec_ = 0.0;

    AudioPump* audioPump_ = nullptr;
    QThread* videoThread_ = nullptr;
    VideoWorker* videoWorker_ = nullptr;
    QTimer* videoTimer_ = nullptr;
    bool framePending_ = false;
    std::atomic<unsigned long long> generation_{1};
};

} // namespace editor::shell
