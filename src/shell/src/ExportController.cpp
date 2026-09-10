#include "shell/ExportController.hpp"
#include "shell/Session.hpp"

#include "export_ffmpeg/Mp4Exporter.hpp"
#include "render/Evaluator.hpp"

#include <QUrl>
#include <QTimer>

namespace editor::shell {

ExportController::ExportController(QObject* parent) : QObject(parent) {}

ExportController::~ExportController() {
    cancelExport();
    if (worker_ != nullptr) {
        worker_->wait(10000);
    }
}

void ExportController::setSession(Session* session) {
    session_ = session;
    if (session_ != nullptr) {
        connect(session_, &Session::projectChanged, this, &ExportController::sessionChanged);
        if (outputPath_.isEmpty()) {
            outputPath_ = session_->filePath().isEmpty()
                              ? QString("out.mp4")
                              : session_->filePath() + ".export.mp4";
            emit outputPathChanged();
        }
    }
}

QString ExportController::summary() const {
    if (session_ == nullptr) {
        return "No project";
    }
    const core::Sequence* seq =
        session_->project().activeSequence();
    if (seq == nullptr) {
        return "No sequence";
    }
    const double dur =
        static_cast<double>(render::Evaluator::sequenceDuration(*seq));
    return QString("%1x%2 @ %3 fps, %4 s  |  H.264 + AAC (.mp4)")
        .arg(static_cast<qulonglong>(seq->width))
        .arg(static_cast<qulonglong>(seq->height))
        .arg(static_cast<double>(seq->fps), 0, 'f', 2)
        .arg(dur, 0, 'f', 2);
}

void ExportController::setOutputPath(const QString& path) {
    if (outputPath_ != path) {
        outputPath_ = path;
        emit outputPathChanged();
    }
}

void ExportController::setOutputUrl(const QUrl& url) {
    const QString local = url.toLocalFile();
    if (!local.isEmpty()) {
        setOutputPath(local);
    }
}

void ExportController::startExport() {
    if (state_ == Running || session_ == nullptr) {
        return;
    }
    const core::Sequence* seq = session_->project().activeSequence();
    if (seq == nullptr) {
        setState(Failed, "No sequence to export");
        return;
    }
    struct Job final : public QThread {
        core::Project project;
        core::Id sequenceId;
        export_ffmpeg::Mp4ExportOptions options;
        std::atomic_bool* cancel = nullptr;
        QString errorText;
        double progressValue = 0.0;
        void run() override {
            export_ffmpeg::Mp4Exporter exporter;
            auto r = exporter.run(project, sequenceId, options, *cancel,
                                  [&](double p) { progressValue = p; });
            if (r.isErr()) {
                errorText = QString::fromStdString(r.error());
            }
            progressValue = r.isOk() ? 1.0 : progressValue;
        }
    };
    cancelExport(); // settle any previous worker first
    if (worker_ != nullptr) {
        worker_->wait(10000);
        worker_->deleteLater();
        worker_ = nullptr;
    }
    cancelFlag_ = new std::atomic_bool(false);
    auto* job = new Job();
    job->project = session_->project();
    job->sequenceId = seq->id;
    job->options.outPath = outputPath_.toStdString();
    job->cancel = cancelFlag_;
    worker_ = job;
    setState(Running);
    connect(job, &QThread::finished, this, [this, job] {
        progress_ = job->progressValue;
        emit progressChanged();
        if (cancelFlag_ != nullptr && cancelFlag_->load()) {
            setState(Cancelled, "Export cancelled");
        } else if (!job->errorText.isEmpty()) {
            setState(Failed, job->errorText);
        } else {
            setState(Done);
        }
        job->deleteLater();
        delete cancelFlag_;
        cancelFlag_ = nullptr;
    });
    // Progress pump (the exporter has no event loop to emit from).
    auto* pump = new QTimer(this);
    connect(pump, &QTimer::timeout, this, [this, job, pump] {
        if (state_ != Running) {
            pump->stop();
            pump->deleteLater();
            return;
        }
        progress_ = job->progressValue;
        emit progressChanged();
    });
    pump->start(100);
    job->start();
}

void ExportController::cancelExport() {
    if (cancelFlag_ != nullptr) {
        cancelFlag_->store(true);
    }
}

void ExportController::setState(int state, const QString& error) {
    state_ = state;
    errorText_ = error;
    emit stateChanged();
}

} // namespace editor::shell
