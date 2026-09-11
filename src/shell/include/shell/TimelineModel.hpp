#pragma once

// Read-only QML view over the active sequence's clips. Refreshed explicitly
// after each command (S1); fine-grained incremental updates are S2 work.

#include <QAbstractListModel>

namespace editor::shell {

class Session;

class TimelineModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(double durationSec READ durationSec NOTIFY modelChanged)
    Q_PROPERTY(double fps READ fps NOTIFY modelChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY modelChanged)
    Q_PROPERTY(QVariantList transitions READ transitions NOTIFY modelChanged)

public:
    enum Roles {
        ClipIdRole = Qt::UserRole + 1,
        TrackIdRole,
        TrackKindRole,
        NameRole,
        StartSecRole,
        DurationSecRole,
        DisplayTextRole,
    };

    explicit TimelineModel(QObject* parent = nullptr);

    void setSession(Session* session);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    double durationSec() const;
    double fps() const;
    QVariantList tracks() const;
    QVariantList transitions() const { return transitions_; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE int rowForClipId(const QString& clipId) const;
    /// Full editable detail for the inspector (live read from the project).
    Q_INVOKABLE QVariantMap clipInfo(const QString& clipId) const;
    Q_INVOKABLE QVariantMap transitionInfo(const QString& transId) const;
    Q_INVOKABLE QString adjacentClipId(const QString& clipId, bool next) const;

signals:
    void modelChanged();

private:
    struct Row {
        QString clipId;
        QString trackId;
        QString trackKind;
        QString name;
        double startSec = 0.0;
        double durationSec = 0.0;
        QString displayText;
    };
    Session* session_ = nullptr;
    std::vector<Row> rows_;
    QVariantList tracks_;
    QVariantList transitions_;
    double durationSec_ = 0.0;
    double fps_ = 30.0;
};

} // namespace editor::shell
