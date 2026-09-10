#include "shell/ThumbnailProvider.hpp"
#include "shell/Session.hpp"

#include "media_ffmpeg/FfmpegThumbnailer.hpp"

namespace editor::shell {

ThumbnailProvider::ThumbnailProvider(Session* session)
    : QQuickImageProvider(QQmlImageProviderBase::Image), session_(session) {}

QImage ThumbnailProvider::requestImage(const QString& id, QSize* size,
                                       const QSize& requestedSize) {
    const int width = requestedSize.width() > 0 ? requestedSize.width() : 256;
    const QString key = id + "@" + QString::number(width);
    {
        QMutexLocker lock(&mutex_);
        if (QImage* hit = cache_.object(key)) {
            if (size != nullptr) {
                *size = hit->size();
            }
            return *hit;
        }
    }
    QImage fallback(16, 16, QImage::Format_RGBA8888);
    fallback.fill(Qt::darkGray);
    if (session_ == nullptr) {
        return fallback;
    }
    const auto it = session_->project().assets.find(id.toStdString());
    if (it == session_->project().assets.end()) {
        return fallback;
    }
    media_ffmpeg::FfmpegThumbnailer grabber;
    if (grabber.open(it->second.path).isErr()) {
        return fallback;
    }
    const double atSec = (std::min)(1.0, static_cast<double>(it->second.duration) / 2.0);
    auto thumb = grabber.grab(rationalFromSeconds(atSec), width);
    if (thumb.isErr()) {
        return fallback;
    }
    QImage img(thumb.value().rgba.data(), thumb.value().width, thumb.value().height,
               thumb.value().width * 4, QImage::Format_RGBA8888);
    QImage owned = img.copy(); // detach from the decode buffer
    if (size != nullptr) {
        *size = owned.size();
    }
    {
        QMutexLocker lock(&mutex_);
        cache_.insert(key, new QImage(owned));
    }
    return owned;
}

} // namespace editor::shell
