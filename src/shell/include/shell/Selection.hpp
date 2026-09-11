#pragma once

// UI-only state: selection, playhead, timeline zoom. Never persisted —
// the document stays clean of view state by design.

#include <QObject>
#include <QString>

namespace editor::shell {

class Selection final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString selectedClipId READ selectedClipId WRITE setSelectedClipId NOTIFY
                   selectionChanged)
    Q_PROPERTY(QString selectedTransitionId READ selectedTransitionId WRITE
                   setSelectedTransitionId NOTIFY transitionSelectionChanged)
    Q_PROPERTY(double playheadSec READ playheadSec WRITE setPlayheadSec NOTIFY playheadChanged)
    Q_PROPERTY(double pxPerSec READ pxPerSec WRITE setPxPerSec NOTIFY zoomChanged)

public:
    explicit Selection(QObject* parent = nullptr);

    QString selectedClipId() const { return selectedClipId_; }
    QString selectedTransitionId() const { return selectedTransitionId_; }
    double playheadSec() const { return playheadSec_; }
    double pxPerSec() const { return pxPerSec_; }

    Q_INVOKABLE void select(const QString& clipId);
    Q_INVOKABLE void selectTransition(const QString& transId);
    Q_INVOKABLE void clearSelection();

public slots:
    void setSelectedClipId(const QString& id);
    void setSelectedTransitionId(const QString& id);
    void setPlayheadSec(double sec);
    void setPxPerSec(double px);
    void zoomIn();
    void zoomOut();

signals:
    void selectionChanged();
    void transitionSelectionChanged();
    void playheadChanged();
    void zoomChanged();

private:
    QString selectedClipId_;
    QString selectedTransitionId_;
    double playheadSec_ = 0.0;
    double pxPerSec_ = 48.0;
};

} // namespace editor::shell
