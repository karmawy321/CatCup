#pragma once

// Preview surface: paints the latest composed QImage aspect-fit. Frame
// production happens elsewhere (FrameCompositor, any thread); this item only
// presents. Scrub path is synchronous (renderAt); playback pushes frames.

#include "shell/FrameCompositor.hpp"

#include <QImage>
#include <QtQuick/QQuickPaintedItem>

namespace editor::shell {

class Session;

// NOTE: deliberately not `final` — qmlRegisterType requires inheriting from
// the registered type (QQmlElement<T> derives from T).
class PreviewItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(double positionSec READ positionSec NOTIFY frameChanged)

public:
    explicit PreviewItem(QQuickItem* parent = nullptr);

    // Q_INVOKABLE so PreviewView.qml can bind the session after creation.
    // Takes QObject* (not Session*): QML marshals QObject natively, while a
    // Session* parameter would require QML type registration for a C++-owned
    // object the UI never constructs. The cast is validated inside.
    Q_INVOKABLE void setSession(QObject* session);
    double positionSec() const { return positionSec_; }

    /// Synchronous scrub render (GUI thread; 720p mpeg4 seek is ~ms).
    Q_INVOKABLE void renderAt(double seconds);
    /// Playback path: invoked (queued) from the player worker thread.
    Q_INVOKABLE void showImage(const QImage& image, double seconds);

    void paint(QPainter* painter) override;

signals:
    void frameChanged();

private:
    Session* session_ = nullptr;
    FrameCompositor compositor_; // GUI thread only (renderAt path)
    QImage current_;
    double positionSec_ = 0.0;
};

} // namespace editor::shell
