#pragma once

// Asset list model for the media browser (names, kinds, durations).
// Pixels come from the "thumb" image provider, not from this model.

#include <QAbstractListModel>

namespace editor::shell {

class Session;

class MediaLibrary final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles { AssetIdRole = Qt::UserRole + 1, NameRole, KindRole, DurationSecRole, PathRole };

    explicit MediaLibrary(QObject* parent = nullptr);

    void setSession(Session* session);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString assetIdAt(int row) const;

private:
    struct Row {
        QString assetId;
        QString name;
        QString kind;
        double durationSec = 0.0;
        QString path;
    };
    Session* session_ = nullptr;
    std::vector<Row> rows_;
};

} // namespace editor::shell
