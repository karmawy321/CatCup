#include "shell/ThumbnailProvider.hpp"
#include "shell/Session.hpp"

#include "media_ffmpeg/FfmpegThumbnailer.hpp"

#include <QDir>
#include <filesystem>

namespace editor::shell {

ThumbnailProvider::ThumbnailProvider(Session* session)
    : QQuickImageProvider(QQmlImageProviderBase::Image), session_(session) {
    if (session_ != nullptr) {
        QObject::connect(session_, &Session::projectChanged, [this]() {
            updateSnapshots();
        });
        updateSnapshots();
    }
}

void ThumbnailProvider::invalidateCache() {
    QMutexLocker lock(&cacheMutex_);
    cache_.clear();
}

void ThumbnailProvider::invalidateAsset(const std::string& assetId) {
    QMutexLocker lock(&cacheMutex_);
    const auto keys = cache_.keys();
    const QString prefix = QString::fromStdString(assetId) + "@";
    for (const QString& k : keys) {
        if (k.startsWith(prefix)) {
            cache_.remove(k);
        }
    }
}

void ThumbnailProvider::updateSnapshots() {
    if (session_ == nullptr) {
        invalidateCache();
        QWriteLocker lock(&snapshotLock_);
        snapshots_.clear();
        return;
    }
    std::unordered_map<std::string, AssetSnapshot> newSnapshots;
    const auto& assets = session_->project().assets;
    for (const auto& [id, a] : assets) {
        AssetSnapshot snap;
        snap.id = id;
        snap.path = a.path;
        snap.duration = a.duration;
        std::error_code ec;
        auto fsPath = std::filesystem::path(QDir::toNativeSeparators(QString::fromStdString(a.path)).toStdWString());
        if (std::filesystem::exists(fsPath, ec)) {
            ec.clear();
            snap.fileSize = static_cast<int64_t>(std::filesystem::file_size(fsPath, ec));
            ec.clear();
            auto lw = std::filesystem::last_write_time(fsPath, ec);
            if (!ec) {
                snap.mtime = lw.time_since_epoch().count();
            }
        }
        newSnapshots[id] = snap;
    }

    {
        QReadLocker rlock(&snapshotLock_);
        for (const auto& [oldId, oldSnap] : snapshots_) {
            auto it = newSnapshots.find(oldId);
            if (it == newSnapshots.end() || it->second.path != oldSnap.path ||
                it->second.fileSize != oldSnap.fileSize || it->second.mtime != oldSnap.mtime) {
                invalidateAsset(oldId);
            }
        }
    }

    {
        QWriteLocker wlock(&snapshotLock_);
        snapshots_ = std::move(newSnapshots);
    }
}

QImage ThumbnailProvider::requestImage(const QString& id, QSize* size,
                                       const QSize& requestedSize) {
    const int width = requestedSize.width() > 0 ? requestedSize.width() : 256;
    QString assetId = id;
    double reqTimeSec = -1.0;
    if (id.contains('?')) {
        assetId = id.section('?', 0, 0);
        const QString query = id.section('?', 1);
        const QStringList params = query.split('&');
        for (const QString& p : params) {
            if (p.startsWith("time=") || p.startsWith("t=")) {
                bool ok = false;
                double val = p.section('=', 1).toDouble(&ok);
                if (ok && val >= 0.0) {
                    reqTimeSec = val;
                }
            }
        }
    }

    const QString key = assetId + "@" + QString::number(reqTimeSec, 'f', 2) + "@" + QString::number(width);
    {
        QMutexLocker lock(&cacheMutex_);
        if (QImage* hit = cache_.object(key)) {
            if (size != nullptr) {
                *size = hit->size();
            }
            return *hit;
        }
    }

    auto getFallback = [&]() {
        const int targetW = requestedSize.width() > 0 ? requestedSize.width() : width;
        const int targetH = requestedSize.height() > 0 ? requestedSize.height() : (targetW * 9 / 16);
        QImage fb(targetW, (std::max)(1, targetH), QImage::Format_RGBA8888);
        fb.fill(Qt::darkGray);
        return fb;
    };

    AssetSnapshot snap;
    bool hasSnap = false;
    {
        QReadLocker rlock(&snapshotLock_);
        auto it = snapshots_.find(assetId.toStdString());
        if (it != snapshots_.end()) {
            snap = it->second;
            hasSnap = true;
        }
    }

    if (!hasSnap || snap.path.empty()) {
        QImage fb = getFallback();
        if (size != nullptr) *size = fb.size();
        return fb;
    }

    media_ffmpeg::FfmpegThumbnailer grabber;
    if (grabber.open(snap.path).isErr()) {
        QImage fb = getFallback();
        if (size != nullptr) *size = fb.size();
        return fb;
    }
    double atSec = reqTimeSec >= 0.0
                       ? reqTimeSec
                       : (std::min)(1.0, static_cast<double>(snap.duration) / 2.0);
    const double dur = static_cast<double>(snap.duration);
    if (dur > 0.0 && atSec > dur) {
        atSec = (std::max)(0.0, dur - 0.05);
    }
    auto thumb = grabber.grab(rationalFromSeconds(atSec), width);
    if (thumb.isErr()) {
        QImage fb = getFallback();
        if (size != nullptr) *size = fb.size();
        return fb;
    }
    QImage img(thumb.value().rgba.data(), thumb.value().width, thumb.value().height,
               thumb.value().width * 4, QImage::Format_RGBA8888);
    QImage owned = img.copy(); // detach from the decode buffer
    if (size != nullptr) {
        *size = owned.size();
    }
    {
        QMutexLocker lock(&cacheMutex_);
        cache_.insert(key, new QImage(owned));
    }
    return owned;
}

} // namespace editor::shell
