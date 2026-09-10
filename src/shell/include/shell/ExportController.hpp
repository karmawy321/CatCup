#pragma once

// Export dialog backend: runs Mp4Exporter on a worker thread with progress
// and cancellation. The dialog binds to state/progress/error.

#include <QObject>
#include <QString>
#include <QThread>

#include <atomic>

namespace editor::shell {

class Session;

class ExportController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY sessionChanged)

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
    QString summary() const;

    Q_INVOKABLE void startExport();
    Q_INVOKABLE void cancelExport();
    /// File-dialog urls arrive as file:// — convert centrally, not in QML.
    Q_INVOKABLE void setOutputUrl(const QUrl& url);

public slots:
    void setOutputPath(const QString& path);

signals:
    void stateChanged();
    void progressChanged();
    void outputPathChanged();
    void sessionChanged();

private:
    void setState(int state, const QString& error = {});

    Session* session_ = nullptr;
    int state_ = Idle;
    double progress_ = 0.0;
    QString errorText_;
    QString outputPath_;
    QThread* worker_ = nullptr;
    std::atomic_bool* cancelFlag_ = nullptr;
};

} // namespace editor::shell
