#pragma once

// Session: owns the Project document, the UndoStack, and the file binding.
// QML performs NO direct model mutation — every edit flows through here into
// commands (undoable, autosave-friendly, assistant-compatible).

#include "commands/Command.hpp"
#include "core/ChangeBus.hpp"
#include "core/Model.hpp"

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>

namespace editor::shell {

core::Rational rationalFromSeconds(double seconds);

class Session final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY projectChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY projectChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY projectChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY projectChanged)
    Q_PROPERTY(bool rippleMode READ rippleMode WRITE setRippleMode NOTIFY rippleModeChanged)
    Q_PROPERTY(bool snappingEnabled READ snappingEnabled WRITE setSnappingEnabled NOTIFY snappingEnabledChanged)

public:
    explicit Session(QObject* parent = nullptr);

    QString projectName() const;
    QString filePath() const { return filePath_; }
    bool dirty() const { return dirty_; }
    bool canUndo() const;
    bool canRedo() const;
    bool rippleMode() const { return rippleMode_; }
    Q_INVOKABLE void setRippleMode(bool v);
    bool snappingEnabled() const { return snappingEnabled_; }
    Q_INVOKABLE void setSnappingEnabled(bool v);

    core::Project& project() { return project_; }
    const core::Project& project() const { return project_; }
    commands::UndoStack& undoStack() { return undo_; }
    core::ChangeBus& bus() { return bus_; }

    Q_INVOKABLE void newProject();
    Q_INVOKABLE bool openFile(const QUrl& url);
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool saveAs(const QUrl& url);
    Q_INVOKABLE bool saveRecovery();
    Q_INVOKABLE bool hasRecovery() const;
    Q_INVOKABLE QVariantMap recoveryInfo() const;
    Q_INVOKABLE bool restoreRecovery();
    Q_INVOKABLE void discardRecovery();
    void clearRecovery();
    static void setRecoveryDirectoryForTesting(const QString& dirPath);
    static QString recoveryDirPath();
    /// Probe + register an asset. Returns the asset id, or "" on failure.
    Q_INVOKABLE QString importMedia(const QUrl& url);
    /// Append the asset as a clip at the end of its track. Returns clip id.
    Q_INVOKABLE QString addClipToTimeline(const QString& assetId);
    Q_INVOKABLE QString addTitle(const QString& text);
    Q_INVOKABLE bool splitSelectedAtPlayhead(const QString& clipId, double playheadSec);
    Q_INVOKABLE bool deleteClip(const QString& clipId);
    Q_INVOKABLE bool moveClipTo(const QString& clipId, double newStartSec);
    Q_INVOKABLE bool moveClipToTrack(const QString& clipId, const QString& newTrackId, double newStartSec);
    Q_INVOKABLE bool trimClip(const QString& clipId, double newInSec, double newOutSec,
                              double newStartSec);
    Q_INVOKABLE bool setClipTransform(const QString& clipId, double scale, double x,
                                      double y, double rotationDeg);
    Q_INVOKABLE bool setClipOpacity(const QString& clipId, double opacity);
    Q_INVOKABLE bool setClipSpeed(const QString& clipId, double speed);
    Q_INVOKABLE bool addClipEffect(const QString& clipId, const QString& effectType);
    Q_INVOKABLE bool removeClipEffect(const QString& clipId, int effectIndex);
    Q_INVOKABLE bool updateClipEffectParam(const QString& clipId, int effectIndex,
                                          const QString& paramName, double paramValue);
    Q_INVOKABLE bool updateClipEffectStrParam(const QString& clipId, int effectIndex,
                                             const QString& paramName, const QString& paramValue);
    Q_INVOKABLE bool setClipColorAdjust(const QString& clipId, double brightness,
                                        double contrast, double saturation,
                                        double temp, double tint);
    Q_INVOKABLE bool setClipText(const QString& clipId, const QString& text,
                                 const QString& fontFamily, double fontSizePt);
    Q_INVOKABLE double snapTime(double targetSec, double thresholdSec = 0.2) const;
    Q_INVOKABLE QString addTransition(const QString& trackId, const QString& fromClipId,
                                      const QString& toClipId, const QString& type,
                                      double durationSec = 1.0, int alignment = 0);
    Q_INVOKABLE bool removeTransition(const QString& transitionId);
    Q_INVOKABLE bool updateTransition(const QString& transitionId, double durationSec,
                                      int alignment, const QString& type,
                                      const QString& easing = "linear");
    Q_INVOKABLE bool autoSilenceCut(const QString& clipId, double thresholdDb = -35.0,
                                    double minDurationSec = 0.4);
    Q_INVOKABLE bool autoSceneSplit(const QString& clipId, double threshold = 0.25);
    Q_INVOKABLE bool generateAutoCaptions(const QString& transcript, int wordsPerCue = 4);
    Q_INVOKABLE bool importSubtitles(const QString& srtContent);
    Q_INVOKABLE bool importSubtitlesFile(const QUrl& url);
    Q_INVOKABLE QString exportSubtitles() const;
    Q_INVOKABLE bool exportSubtitlesFile(const QUrl& url) const;
    Q_INVOKABLE bool setClipKeyframe(const QString& clipId, double seqTimeSec,
                                     double scale, double x, double y,
                                     double rotationDeg, double opacity,
                                     const QString& easing = "linear");
    Q_INVOKABLE bool removeClipKeyframe(const QString& clipId, double seqTimeSec);
    Q_INVOKABLE bool setClipFade(const QString& clipId, double fadeInSec, double fadeOutSec);
    Q_INVOKABLE bool setSequenceFormat(int width, int height);
    Q_INVOKABLE bool setSequenceAspectPreset(const QString& preset);
    Q_INVOKABLE QString getSequenceAspectPreset() const;
    Q_INVOKABLE bool normalizeClipAudio(const QString& clipId, double targetLufs = -14.0);
    Q_INVOKABLE bool denoiseClipAudio(const QString& clipId, double rumbleCutoffHz = 80.0);
    Q_INVOKABLE bool extractAudioFromClip(const QString& clipId);
    Q_INVOKABLE QString extractAudioToFile(const QString& clipId, const QString& destinationPath = "");
    Q_INVOKABLE bool clipHasAudio(const QString& clipId) const;
    Q_INVOKABLE bool setTrackMuted(const QString& trackId, bool muted);
    Q_INVOKABLE bool setClipBlendMode(const QString& clipId, const QString& mode);
    Q_INVOKABLE bool setClipHighlightsShadows(const QString& clipId, double highlights, double shadows);
    Q_INVOKABLE bool setClipColorWheels(const QString& clipId, double liftY, double gammaY, double gainY,
                                        double offsetR, double offsetG, double offsetB, double lumaMix);
    Q_INVOKABLE bool importCapCutDraft(const QUrl& url);
    Q_INVOKABLE bool exportCapCutDraft(const QUrl& url);
    Q_INVOKABLE bool relinkAsset(const QString& assetId, const QString& newPath);
    Q_INVOKABLE int sequenceWidth() const;
    Q_INVOKABLE int sequenceHeight() const;
    Q_INVOKABLE double sequenceFps() const;
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void projectChanged();
    void rippleModeChanged();
    void snappingEnabledChanged();
    void autosaveFailed(const QString& message);
    void error(const QString& message);

private:
    bool execute(std::unique_ptr<commands::ICommand> cmd);
    void ensureTimelineTracks(core::Sequence& seq);
    core::Track* findTrackForKind(core::Sequence& seq, core::TrackKind kind);
    core::Rational trackEnd(const core::Sequence& seq, const core::Track& track) const;

    core::Project project_;
    commands::UndoStack undo_{100, &bus_};
    core::ChangeBus bus_;
    QString filePath_;
    bool dirty_ = false;
    bool rippleMode_ = false;
    bool snappingEnabled_ = true;
};

} // namespace editor::shell
