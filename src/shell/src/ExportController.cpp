#include "shell/ExportController.hpp"
#include "shell/Session.hpp"

#include "export_ffmpeg/Mp4Exporter.hpp"
#include "render/Evaluator.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include <filesystem>

namespace editor::shell {

namespace {

bool isSameFilesystemTarget(const QString& p1, const QString& p2) {
    if (p1.trimmed().isEmpty() || p2.trimmed().isEmpty()) {
        return false;
    }
    std::filesystem::path fs1(QDir::toNativeSeparators(p1).toStdWString());
    std::filesystem::path fs2(QDir::toNativeSeparators(p2).toStdWString());

    std::error_code ec;
    // 1. If both exist on disk, check physical filesystem identity
    if (std::filesystem::exists(fs1, ec) && std::filesystem::exists(fs2, ec)) {
        ec.clear();
        if (std::filesystem::equivalent(fs1, fs2, ec)) {
            return true;
        }
    }

    // 2. Canonical / weakly_canonical path resolution
    ec.clear();
    auto c1 = std::filesystem::weakly_canonical(fs1, ec);
    if (ec) c1 = fs1;
    ec.clear();
    auto c2 = std::filesystem::weakly_canonical(fs2, ec);
    if (ec) c2 = fs2;

    ec.clear();
    if (std::filesystem::exists(c1, ec) && std::filesystem::exists(c2, ec)) {
        ec.clear();
        if (std::filesystem::equivalent(c1, c2, ec)) {
            return true;
        }
    }

    // 3. String comparison with canonical paths (case-insensitive on Windows)
    QString s1 = QDir::cleanPath(QString::fromStdWString(c1.wstring()));
    QString s2 = QDir::cleanPath(QString::fromStdWString(c2.wstring()));
#ifdef _WIN32
    return QString::compare(s1, s2, Qt::CaseInsensitive) == 0;
#else
    return s1 == s2;
#endif
}

} // namespace

ExportController::ExportController(QObject* parent) : QObject(parent) {}

ExportController::~ExportController() {
    stopAndCleanWorker(2000);
}

void ExportController::stopAndCleanWorker(unsigned long waitMs) {
    if (progressTimer_ != nullptr) {
        progressTimer_->stop();
        progressTimer_->disconnect();
        progressTimer_->deleteLater();
        progressTimer_ = nullptr;
    }
    cancelExport();
    if (worker_ != nullptr) {
        worker_->disconnect(this);
        bool finished = worker_->wait(waitMs);
        if (finished) {
            delete worker_.data();
        } else {
            // Never delete a still-running QThread; let it self-delete upon termination
            connect(worker_.data(), &QThread::finished, worker_.data(), &QObject::deleteLater);
        }
        worker_ = nullptr;
    }
    cancelFlag_.reset();
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

void ExportController::setExportWidth(int w) {
    if (exportWidth_ != w) {
        exportWidth_ = w;
        emit exportResolutionChanged();
        emit sessionChanged();
    }
}

void ExportController::setExportHeight(int h) {
    if (exportHeight_ != h) {
        exportHeight_ = h;
        emit exportResolutionChanged();
        emit sessionChanged();
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
    const int outW = exportWidth_ > 0 ? exportWidth_ : static_cast<int>(seq->width);
    const int outH = exportHeight_ > 0 ? exportHeight_ : static_cast<int>(seq->height);
    return QString("%1x%2 @ %3 fps, %4 s  |  H.264 + AAC (.mp4)")
        .arg(outW)
        .arg(outH)
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

QString ExportController::validateDestination(const QString& path) const {
    if (path.trimmed().isEmpty()) {
        return "Destination path cannot be empty.";
    }
    if (session_ != nullptr) {
        if (!session_->filePath().isEmpty()) {
            if (isSameFilesystemTarget(path, session_->filePath())) {
                return "Output destination cannot overwrite the project file.";
            }
        }
        for (const auto& [aid, a] : session_->project().assets) {
            if (!a.path.empty()) {
                if (isSameFilesystemTarget(path, QString::fromStdString(a.path))) {
                    return QString("Output destination cannot overwrite source asset: %1")
                        .arg(QString::fromStdString(a.path));
                }
            }
        }
    }
    QFileInfo fi(path);
    if (fi.exists()) {
        return "EXISTS";
    }
    return "";
}

void ExportController::openCompletedFile() const {
    if (!completedOutputPath_.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(completedOutputPath_));
    }
}

void ExportController::openCompletedFolder() const {
    if (!completedOutputPath_.isEmpty()) {
        QFileInfo fi(completedOutputPath_);
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
    }
}

void ExportController::reset() {
    if (state_ != Running) {
        setState(Idle);
        progress_ = 0.0;
        emit progressChanged();
    }
}

void ExportController::startExport(bool confirmOverwrite) {
    if (state_ == Running || session_ == nullptr) {
        return;
    }
    const core::Sequence* seq = session_->project().activeSequence();
    if (seq == nullptr) {
        setState(Failed, "No sequence to export");
        return;
    }

    QString validation = validateDestination(outputPath_);
    if (validation == "EXISTS") {
        if (!confirmOverwrite) {
            setState(Failed, "File already exists. Overwrite not confirmed.");
            return;
        }
    } else if (!validation.isEmpty()) {
        setState(Failed, validation);
        return;
    }

    struct Job final : public QThread {
        core::Project project;
        core::Id sequenceId;
        export_ffmpeg::Mp4ExportOptions options;
        std::shared_ptr<std::atomic_bool> cancel;
        QString errorText;
        bool committed = false;
        QString jobDestination;
        std::atomic<double> progressValue{0.0};
        void run() override {
            export_ffmpeg::Mp4Exporter exporter;
            auto r = exporter.run(project, sequenceId, options, *cancel,
                                  [&](double p) { progressValue.store(p); });
            if (r.isErr()) {
                errorText = QString::fromStdString(r.error());
            }
            committed = r.isOk();
            if (committed) {
                progressValue.store(1.0);
            }
        }
    };

    stopAndCleanWorker(10000);

    cancelFlag_ = std::make_shared<std::atomic_bool>(false);
    const QString targetDest = QFileInfo(outputPath_).absoluteFilePath();
    activeJobDestination_ = targetDest;

    auto* job = new Job();
    job->project = session_->project();
    job->sequenceId = seq->id;
    job->options.outPath = targetDest.toStdString();
    job->options.overrideWidth = exportWidth_;
    job->options.overrideHeight = exportHeight_;
    job->options.allowOverwrite = confirmOverwrite;
    job->cancel = cancelFlag_;
    job->jobDestination = targetDest;
    worker_ = job;
    setState(Running);

    connect(job, &QThread::finished, this, [this, job] {
        // Disconnect and stop progress timer BEFORE publishing terminal state or deleting job
        if (progressTimer_ != nullptr) {
            progressTimer_->stop();
            progressTimer_->disconnect();
            progressTimer_->deleteLater();
            progressTimer_ = nullptr;
        }

        progress_ = job->progressValue.load();
        emit progressChanged();

        const bool committed = job->committed;
        const QString err = job->errorText;
        const QString dest = job->jobDestination;

        if (worker_ == job) {
            worker_ = nullptr;
        }
        job->deleteLater();

        if (committed) {
            completedOutputPath_ = dest;
            emit completedOutputPathChanged();
            setState(Done);
        } else if (err == "cancelled") {
            setState(Cancelled, "Export cancelled");
        } else {
            setState(Failed, err);
        }
    });

    // Progress pump
    auto* pump = new QTimer(this);
    progressTimer_ = pump;
    connect(pump, &QTimer::timeout, this, [this, job] {
        if (state_ != Running || worker_ != job) {
            if (progressTimer_ != nullptr) {
                progressTimer_->stop();
                progressTimer_->disconnect();
                progressTimer_->deleteLater();
                progressTimer_ = nullptr;
            }
            return;
        }
        progress_ = job->progressValue.load();
        emit progressChanged();
    });
    pump->start(100);
    job->start();
}

void ExportController::cancelExport() {
    if (cancelFlag_) {
        cancelFlag_->store(true);
    }
}

void ExportController::setState(int state, const QString& error) {
    state_ = state;
    errorText_ = error;
    emit stateChanged();
}

} // namespace editor::shell
