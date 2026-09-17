#pragma once

// QML image provider "thumb": thumb://<assetId> -> filmstrip still.
// Synchronous grab (cached); the async behavior QML expects comes from
// Image's async loading, not from threads here.

#include "core/Rational.hpp"

#include <QCache>
#include <QMutex>
#include <QReadWriteLock>
#include <QtQuick/QQuickImageProvider>
#include <string>
#include <unordered_map>

namespace editor::shell {

class Session;

struct AssetSnapshot {
    std::string id;
    std::string path;
    core::Rational duration;
    int64_t fileSize = 0;
    int64_t mtime = 0;
};

class ThumbnailProvider final : public QQuickImageProvider {
public:
    explicit ThumbnailProvider(Session* session);

    QImage requestImage(const QString& id, QSize* size,
                        const QSize& requestedSize) override;

    void updateSnapshots();
    void invalidateCache();
    void invalidateAsset(const std::string& assetId);

private:
    Session* session_;
    mutable QMutex cacheMutex_;
    mutable QReadWriteLock snapshotLock_;
    QCache<QString, QImage> cache_{256};
    std::unordered_map<std::string, AssetSnapshot> snapshots_;
};

} // namespace editor::shell
