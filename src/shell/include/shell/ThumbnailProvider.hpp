#pragma once

// QML image provider "thumb": thumb://<assetId> -> filmstrip still.
// Synchronous grab (cached); the async behavior QML expects comes from
// Image's async loading, not from threads here.

#include <QCache>
#include <QMutex>
#include <QtQuick/QQuickImageProvider>

namespace editor::shell {

class Session;

class ThumbnailProvider final : public QQuickImageProvider {
public:
    explicit ThumbnailProvider(Session* session);

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

private:
    Session* session_;
    QMutex mutex_;
    QCache<QString, QImage> cache_{64};
};

} // namespace editor::shell
