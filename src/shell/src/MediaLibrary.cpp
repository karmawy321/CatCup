#include "shell/MediaLibrary.hpp"
#include "shell/Session.hpp"

#include <QFileInfo>

namespace editor::shell {

MediaLibrary::MediaLibrary(QObject* parent) : QAbstractListModel(parent) {}

void MediaLibrary::setSession(Session* session) {
    session_ = session;
    if (session_ != nullptr) {
        connect(session_, &Session::projectChanged, this, &MediaLibrary::refresh);
    }
    refresh();
}

int MediaLibrary::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant MediaLibrary::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(rows_.size())) {
        return {};
    }
    const Row& r = rows_[static_cast<size_t>(index.row())];
    switch (role) {
    case AssetIdRole: return r.assetId;
    case NameRole: return r.name;
    case KindRole: return r.kind;
    case DurationSecRole: return r.durationSec;
    case PathRole: return r.path;
    case IsMissingRole: return r.isMissing;
    default: return {};
    }
}

QHash<int, QByteArray> MediaLibrary::roleNames() const {
    return {
        {AssetIdRole, "assetId"},
        {NameRole, "assetName"},
        {KindRole, "assetKind"},
        {DurationSecRole, "durationSec"},
        {PathRole, "assetPath"},
        {IsMissingRole, "isMissing"},
    };
}

void MediaLibrary::refresh() {
    beginResetModel();
    rows_.clear();
    if (session_ != nullptr) {
        for (const auto& [id, asset] : session_->project().assets) {
            Row r;
            r.assetId = QString::fromStdString(id);
            r.name = QFileInfo(QString::fromStdString(asset.path)).fileName();
            r.kind = asset.kind == core::AssetKind::Video
                         ? "video"
                         : (asset.kind == core::AssetKind::Audio ? "audio" : "image");
            r.durationSec = static_cast<double>(asset.duration);
            r.path = QString::fromStdString(asset.path);
            r.isMissing = !r.path.isEmpty() && !QFile::exists(r.path);
            rows_.push_back(std::move(r));
        }
    }
    endResetModel();
}

QString MediaLibrary::assetIdAt(int row) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return {};
    }
    return rows_[static_cast<size_t>(row)].assetId;
}

bool MediaLibrary::isMissing(int row) const {
    if (row < 0 || row >= static_cast<int>(rows_.size())) {
        return false;
    }
    return rows_[static_cast<size_t>(row)].isMissing;
}

bool MediaLibrary::relinkAsset(const QString& assetId, const QString& newPath) {
    if (session_ == nullptr) return false;
    bool ok = session_->relinkAsset(assetId, newPath);
    if (ok) refresh();
    return ok;
}

} // namespace editor::shell
