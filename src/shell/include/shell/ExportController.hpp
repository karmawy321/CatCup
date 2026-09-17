#pragma once

// Export dialog backend: runs Mp4Exporter on a worker thread with progress
// and cancellation. The dialog binds to state/progress/error.

#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <memory>

namespace editor::shell {

class Session;

class ExportController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY sessionChanged)

    Q_PROPERTY(int exportWidth READ exportWidth WRITE setExportWidth NOTIFY exportResolutionChanged)
    Q_PROPERTY(int exportHeight READ exportHeight WRITE setExportHeight NOTIFY exportResolutionChanged)
    Q_PROPERTY(QString completedOutputPath READ completedOutputPath NOTIFY completedOutputPathChanged)

public:
    enum State { Idle = 0, Running = 1, Done = 2, Cancelled = 3, Failed = 4 };
    Q_ENUM(State)

    explicit ExportController(QObject* parent = nullptr);
    ~ExportController() override;

    void setSession(Session* session);

    int state() const { return state_; }
    double progress() const { return progress_; }
    QString errorText() const { return errorText_; }
    QString outputPath() const { return outputPath_; }
    QString completedOutputPath() const { return completedOutputPath_; }
    int exportWidth() const { return exportWidth_; }
    void setExportWidth(int w);
    int exportHeight() const { return exportHeight_; }
    void setExportHeight(int h);
    QString summary() const;

    Q_INVOKABLE void startExport(bool confirmOverwrite = false);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void reset();
    Q_INVOKABLE QString validateDestination(const QString& path) const;
    Q_INVOKABLE void openCompletedFile() const;
    Q_INVOKABLE void openCompletedFolder() const;
    /// File-dialog urls arrive as file:// — convert centrally, not in QML.
    Q_INVOKABLE void setOutputUrl(const QUrl& url);

public slots:
    void setOutputPath(const QString& path);

signals:
    void stateChanged();
    void progressChanged();
    void outputPathChanged();
    void completedOutputPathChanged();
    void exportResolutionChanged();
    void sessionChanged();

private:
    void setState(int state, const QString& error = {});
    void stopAndCleanWorker(unsigned long waitMs = 10000);

    Session* session_ = nullptr;
    int state_ = Idle;
    double progress_ = 0.0;
    QString errorText_;
    QString outputPath_;
    QString completedOutputPath_;
    QString activeJobDestination_;
    int exportWidth_ = 0;
    int exportHeight_ = 0;
    QPointer<QThread> worker_;
    QPointer<QTimer> progressTimer_;
    std::shared_ptr<std::atomic_bool> cancelFlag_;
};

} // namespace editor::shell
