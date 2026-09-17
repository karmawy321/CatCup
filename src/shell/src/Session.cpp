#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "shell/Session.hpp"

#include <algorithm>
#include "commands/ClipCommands.hpp"
#include "commands/EffectCommands.hpp"
#include "commands/SmartCommands.hpp"
#include "commands/TransitionCommands.hpp"
#include "effects/EffectSchema.hpp"
#include "ai/AudioAnalysis.hpp"
#include "ai/CaptionEngine.hpp"
#include "ai/SceneDetection.hpp"
#include "core/Ids.hpp"
#include "media_ffmpeg/FfmpegProber.hpp"
#include "media_ffmpeg/FrameDecoder.hpp"
#include "persist/ProjectSerializer.hpp"
#include "persist/Json.hpp"
#include "persist/CapCutDraftBridge.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <cmath>

namespace editor::shell {

core::Rational rationalFromSeconds(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) {
        return core::Rational(0);
    }
    const auto micros = static_cast<std::int64_t>(std::llround(seconds * 1000000.0));
    return core::Rational(micros, 1000000);
}

Session::Session(QObject* parent) : QObject(parent), undo_(100, &bus_) {
    newProject();
}

QString Session::projectName() const {
    return QString::fromStdString(project_.name);
}

bool Session::canUndo() const {
    return undo_.canUndo();
}

bool Session::canRedo() const {
    return undo_.canRedo();
}

namespace {

QString g_testRecoveryDir;

QString legacyRecoveryProjectFilePath() {
    return Session::recoveryDirPath() + "/catcup_recovery.json";
}

QString legacyRecoveryMetaFilePath() {
    return Session::recoveryDirPath() + "/catcup_recovery_meta.json";
}

QString currentPointerFilePath() {
    return Session::recoveryDirPath() + "/current.ptr";
}

struct RecoveryData {
    bool valid = false;
    int generation = 0;
    QString filename;
    QString originalFilePath;
    QString projectName;
    QString timestamp;
    QByteArray projectJson;
};

RecoveryData loadActiveRecoveryEnvelope() {
    RecoveryData out;
    const QString dirPath = Session::recoveryDirPath();
    const QString ptrPath = currentPointerFilePath();

    auto verifyGenFile = [&](const QString& fullPath) -> bool {
        QFile gf(fullPath);
        if (!gf.open(QIODevice::ReadOnly)) return false;
        QJsonDocument doc = QJsonDocument::fromJson(gf.readAll());
        gf.close();
        if (!doc.isObject()) return false;
        QJsonObject obj = doc.object();
        QString expectedHash = obj.value("contentHash").toString();
        if (expectedHash.isEmpty()) return false;
        QJsonValue pVal = obj.value("project");
        if (!pVal.isObject()) return false;
        QByteArray pBytes = QJsonDocument(pVal.toObject()).toJson(QJsonDocument::Compact);
        QString actualHash = QString::fromUtf8(QCryptographicHash::hash(pBytes, QCryptographicHash::Sha256).toHex());
        if (actualHash != expectedHash) return false;

        out.valid = true;
        out.generation = obj.value("generation").toInt();
        out.filename = QFileInfo(fullPath).fileName();
        out.originalFilePath = obj.value("originalFilePath").toString();
        out.projectName = obj.value("projectName").toString("Untitled");
        out.timestamp = obj.value("timestamp").toString();
        out.projectJson = pBytes;
        return true;
    };

    // 1. Try pointer file
    if (QFile::exists(ptrPath)) {
        QFile pf(ptrPath);
        if (pf.open(QIODevice::ReadOnly)) {
            QString targetGen = QString::fromUtf8(pf.readAll().trimmed());
            pf.close();
            if (!targetGen.isEmpty()) {
                QString fullPath = dirPath + "/" + targetGen;
                if (QFile::exists(fullPath) && verifyGenFile(fullPath)) {
                    return out;
                }
            }
        }
    }

    // 2. Pointer missing or invalid: scan gen_*.json descending
    QDir dir(dirPath);
    QStringList gens = dir.entryList(QStringList() << "gen_*.json", QDir::Files, QDir::Name | QDir::Reversed);
    for (const QString& g : gens) {
        if (verifyGenFile(dirPath + "/" + g)) {
            return out;
        }
    }

    // 3. Fall back to legacy recovery files
    QString legacyProj = legacyRecoveryProjectFilePath();
    QString legacyMeta = legacyRecoveryMetaFilePath();
    if (QFile::exists(legacyProj) && QFile::exists(legacyMeta)) {
        QFile mf(legacyMeta);
        if (mf.open(QIODevice::ReadOnly)) {
            QJsonDocument mDoc = QJsonDocument::fromJson(mf.readAll());
            mf.close();
            if (mDoc.isObject()) {
                QJsonObject mObj = mDoc.object();
                QString expectedHash = mObj.value("contentHash").toString();
                QFile pf(legacyProj);
                if (pf.open(QIODevice::ReadOnly)) {
                    QByteArray pBytes = pf.readAll();
                    pf.close();
                    QString actualHash = QString::fromUtf8(QCryptographicHash::hash(pBytes, QCryptographicHash::Sha256).toHex());
                    if (!expectedHash.isEmpty() && actualHash == expectedHash) {
                        out.valid = true;
                        out.generation = 0;
                        out.filename = "catcup_recovery.json";
                        out.originalFilePath = mObj.value("originalFilePath").toString();
                        out.projectName = mObj.value("projectName").toString("Untitled");
                        out.timestamp = mObj.value("timestamp").toString();
                        out.projectJson = pBytes;
                        return out;
                    }
                }
            }
        }
    }

    return out;
}

} // namespace

void Session::setRecoveryDirectoryForTesting(const QString& dirPath) {
    g_testRecoveryDir = dirPath;
}

QString Session::recoveryDirPath() {
    if (!g_testRecoveryDir.isEmpty()) {
        return g_testRecoveryDir;
    }
    const QString env = qEnvironmentVariable("CATCUP_RECOVERY_DIR");
    if (!env.isEmpty()) {
        return env;
    }
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/CatCup";
    }
    return base + "/recovery";
}

void Session::newProject() {
    project_ = core::Project{};
    project_.name = "Untitled";
    core::Sequence seq;
    seq.id = core::IdGenerator::make("seq");
    seq.name = "Sequence 01";
    seq.fps = core::Rational(30, 1);
    seq.width = 1280;
    seq.height = 720;
    project_.sequences.push_back(std::move(seq));
    project_.activeSequenceId = project_.sequences.front().id;
    ensureTimelineTracks(project_.sequences.front());
    undo_.clear();
    filePath_.clear();
    dirty_ = false;
    emit projectChanged();
}

bool Session::openFile(const QUrl& url) {
    const QString local = url.toLocalFile();
    auto loaded = persist::loadProject(local.toStdString());
    if (loaded.isErr()) {
        emit error(QString::fromStdString(loaded.error()));
        return false;
    }
    project_ = std::move(loaded.value());
    if (core::Sequence* seq = project_.activeSequence()) {
        ensureTimelineTracks(*seq);
    }
    undo_.clear();
    filePath_ = local;
    dirty_ = false;
    emit projectChanged();
    return true;
}

bool Session::save() {
    if (filePath_.isEmpty()) {
        emit error("No file name — use Save As");
        return false;
    }
    auto r = persist::saveProject(project_, filePath_.toStdString());
    if (r.isErr()) {
        emit error(QString::fromStdString(r.error()));
        return false;
    }
    dirty_ = false;
    clearRecovery();
    emit projectChanged();
    return true;
}

bool Session::saveAs(const QUrl& url) {
    const QString local = url.toLocalFile();
    if (local.isEmpty()) {
        emit error("Invalid destination path");
        return false;
    }
    auto r = persist::saveProject(project_, local.toStdString());
    if (r.isErr()) {
        emit error(QString::fromStdString(r.error()));
        return false;
    }
    filePath_ = local;
    dirty_ = false;
    clearRecovery();
    emit projectChanged();
    return true;
}

bool Session::saveRecovery() {
    const QString dirPath = recoveryDirPath();
    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(".")) {
        emit autosaveFailed("Failed to create recovery directory");
        emit error("Autosave recovery snapshot failed");
        return false;
    }

    // Determine next generation index
    int maxGen = 0;
    const QStringList entryList = dir.entryList(QStringList() << "gen_*.json", QDir::Files);
    for (const QString& f : entryList) {
        QString numStr = f.mid(4, f.length() - 9);
        bool ok = false;
        int g = numStr.toInt(&ok);
        if (ok && g > maxGen) {
            maxGen = g;
        }
    }
    const int nextGen = maxGen + 1;
    const QString genFilename = QString("gen_%1.json").arg(nextGen, 6, 10, QChar('0'));
    const QString genPath = dirPath + "/" + genFilename;

    // 1. Build project JSON string and SHA-256 hash
    auto pRes = persist::projectToJson(project_);
    if (pRes.isErr()) {
        emit autosaveFailed("Failed to serialize project for recovery");
        emit error("Autosave recovery snapshot failed");
        return false;
    }
    const QJsonDocument projectDoc = QJsonDocument::fromJson(
        QByteArray::fromStdString(persist::dumpJson(pRes.value(), false)));
    if (!projectDoc.isObject()) {
        emit autosaveFailed("Failed to serialize project for recovery");
        emit error("Autosave recovery snapshot failed");
        return false;
    }
    const QByteArray projBytes = projectDoc.toJson(QJsonDocument::Compact);
    QString contentHash = QString::fromUtf8(
        QCryptographicHash::hash(projBytes, QCryptographicHash::Sha256).toHex());

    // 2. Build envelope object
    QJsonObject env;
    env["version"] = 2;
    env["generation"] = nextGen;
    env["originalFilePath"] = filePath_;
    env["projectName"] = projectName();
    env["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    env["contentHash"] = contentHash;
    env["project"] = projectDoc.object();

    {
        QSaveFile f(genPath);
        f.setDirectWriteFallback(false);
        if (!f.open(QIODevice::WriteOnly)) {
            emit autosaveFailed("Failed to open temporary recovery snapshot file");
            emit error("Autosave recovery snapshot failed");
            return false;
        }
        const QByteArray envBytes = QJsonDocument(env).toJson(QJsonDocument::Compact);
        if (f.write(envBytes) != envBytes.size()) {
            f.cancelWriting();
            emit autosaveFailed("Failed to write complete recovery snapshot");
            emit error("Autosave recovery snapshot failed");
            return false;
        }
        if (!f.commit()) {
            emit autosaveFailed("Failed to commit recovery generation file");
            emit error("Autosave recovery snapshot failed");
            return false;
        }
    }

    // 5. Update current pointer atomically
    const QString ptrPath = currentPointerFilePath();
    {
        QSaveFile pf(ptrPath);
        pf.setDirectWriteFallback(false);
        if (!pf.open(QIODevice::WriteOnly)) {
            emit autosaveFailed("Failed to write recovery pointer file");
            emit error("Autosave recovery snapshot failed");
            return false;
        }
        const QByteArray ptrBytes = genFilename.toUtf8();
        if (pf.write(ptrBytes) != ptrBytes.size()) {
            pf.cancelWriting();
            emit autosaveFailed("Failed to write complete recovery pointer file");
            emit error("Autosave recovery snapshot failed");
            return false;
        }
        if (!pf.commit()) {
            emit autosaveFailed("Failed to commit recovery pointer file");
            emit error("Autosave recovery snapshot failed");
            return false;
        }
    }

    // 6. Prune older generations: keep current (nextGen) and previous (nextGen - 1)
    for (const QString& f : entryList) {
        QString numStr = f.mid(4, f.length() - 9);
        bool ok = false;
        int g = numStr.toInt(&ok);
        if (ok && g < nextGen - 1) {
            QFile::remove(dirPath + "/" + f);
        }
    }
    // Clean up any stray tmp files
    const QStringList tmps = dir.entryList(QStringList() << "*.tmp", QDir::Files);
    for (const QString& t : tmps) {
        QFile::remove(dirPath + "/" + t);
    }
    return true;
}

bool Session::hasRecovery() const {
    RecoveryData rd = loadActiveRecoveryEnvelope();
    return rd.valid;
}

QVariantMap Session::recoveryInfo() const {
    QVariantMap res;
    res["hasRecovery"] = false;
    res["projectName"] = "Untitled";
    res["originalFilePath"] = "";
    res["timestamp"] = "";

    RecoveryData rd = loadActiveRecoveryEnvelope();
    if (rd.valid) {
        res["hasRecovery"] = true;
        res["projectName"] = rd.projectName;
        res["originalFilePath"] = rd.originalFilePath;
        res["timestamp"] = rd.timestamp;
    }
    return res;
}

bool Session::restoreRecovery() {
    RecoveryData rd = loadActiveRecoveryEnvelope();
    if (!rd.valid) {
        emit error("No valid recovery snapshot found");
        return false;
    }

    auto parsed = persist::parseJson(rd.projectJson.toStdString());
    if (parsed.isErr()) {
        emit error(QString::fromStdString(parsed.error()));
        return false;
    }
    auto loaded = persist::projectFromJson(parsed.value());
    if (loaded.isErr()) {
        emit error(QString::fromStdString(loaded.error()));
        return false;
    }
    project_ = std::move(loaded.value());
    if (core::Sequence* seq = project_.activeSequence()) {
        ensureTimelineTracks(*seq);
    }
    undo_.clear();
    filePath_ = rd.originalFilePath;
    dirty_ = true;
    emit projectChanged();
    return true;
}

void Session::clearRecovery() {
    const QString dirPath = recoveryDirPath();
    QDir dir(dirPath);
    if (!dir.exists()) return;
    QFile::remove(currentPointerFilePath());
    QFile::remove(currentPointerFilePath() + ".tmp");
    QFile::remove(legacyRecoveryProjectFilePath());
    QFile::remove(legacyRecoveryMetaFilePath());
    QFile::remove(legacyRecoveryProjectFilePath() + ".tmp");
    QFile::remove(legacyRecoveryMetaFilePath() + ".tmp");

    const QStringList files = dir.entryList(QStringList() << "gen_*.json" << "*.tmp", QDir::Files);
    for (const QString& f : files) {
        QFile::remove(dirPath + "/" + f);
    }
}

void Session::discardRecovery() {
    clearRecovery();
}

QString Session::importMedia(const QUrl& url) {
    const QString local = url.toLocalFile();
    media_ffmpeg::FfmpegProber prober;
    auto probed = prober.probe(local.toStdString());
    if (probed.isErr()) {
        emit error(QString::fromStdString(probed.error()));
        return {};
    }
    const media::ProbeResult& pr = probed.value();
    core::Asset asset;
    asset.id = core::IdGenerator::make("asset");
    asset.path = local.toStdString();
    asset.duration = pr.duration.num() > 0 ? pr.duration : core::Rational(4, 1);
    asset.fps = pr.fps.num() > 0 ? pr.fps : core::Rational(30, 1);
    asset.width = pr.width;
    asset.height = pr.height;
    asset.hasAudio = pr.hasAudio;
    if (pr.hasVideo) {
        asset.kind = core::AssetKind::Video;
    } else if (pr.hasAudio) {
        asset.kind = core::AssetKind::Audio;
    } else {
        asset.kind = core::AssetKind::Image;
    }
    if (pr.hasVideo && !pr.hasAudio) {
        emit error("Imported media contains no audio stream");
    }
    const QString assetId = QString::fromStdString(asset.id);
    project_.assets.emplace(asset.id, std::move(asset));
    dirty_ = true;
    emit projectChanged();
    return assetId;
}

QString Session::addClipToTimeline(const QString& assetId) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        emit error("No active sequence");
        return {};
    }
    const auto ait = project_.assets.find(assetId.toStdString());
    if (ait == project_.assets.end()) {
        emit error("Unknown asset");
        return {};
    }
    const core::Asset& asset = ait->second;
    ensureTimelineTracks(*seq);

    if (asset.kind == core::AssetKind::Audio) {
        core::Track* track = findTrackForKind(*seq, core::TrackKind::Audio);
        if (track == nullptr) {
            emit error("No audio track on timeline");
            return {};
        }
        core::Clip clip;
        clip.id = core::IdGenerator::make("clip");
        clip.assetId = asset.id;
        clip.name = QFileInfo(QString::fromStdString(asset.path)).completeBaseName().toStdString();
        clip.sourceIn = core::Rational(0);
        clip.sourceOut = asset.duration;
        clip.seqStart = trackEnd(*seq, *track);
        const QString clipId = QString::fromStdString(clip.id);
        if (!execute(commands::makeAddClipCommand(track->id, std::move(clip)))) {
            return {};
        }
        return clipId;
    }

    // Video or Image
    core::Track* vTrack = findTrackForKind(*seq, core::TrackKind::Video);
    if (vTrack == nullptr) {
        emit error("No video track on timeline");
        return {};
    }
    core::Clip vClip;
    vClip.id = core::IdGenerator::make("clip");
    vClip.assetId = asset.id;
    vClip.name = QFileInfo(QString::fromStdString(asset.path)).completeBaseName().toStdString();
    vClip.sourceIn = core::Rational(0);
    vClip.sourceOut = asset.duration;
    vClip.seqStart = trackEnd(*seq, *vTrack);
    const QString clipId = QString::fromStdString(vClip.id);
    if (!execute(commands::makeAddClipCommand(vTrack->id, std::move(vClip)))) {
        return {};
    }

    // If asset also has audio, create matching audio clip on audio track A1
    if (asset.hasAudio) {
        core::Track* aTrack = findTrackForKind(*seq, core::TrackKind::Audio);
        if (aTrack != nullptr) {
            core::Clip aClip;
            aClip.id = core::IdGenerator::make("clip");
            aClip.assetId = asset.id;
            aClip.name = vClip.name + "-audio";
            aClip.sourceIn = core::Rational(0);
            aClip.sourceOut = asset.duration;
            aClip.seqStart = trackEnd(*seq, *aTrack);
            execute(commands::makeAddClipCommand(aTrack->id, std::move(aClip)));
        }
    }
    return clipId;
}

QString Session::addTitle(const QString& text) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        emit error("No active sequence");
        return {};
    }
    core::Track* track = findTrackForKind(*seq, core::TrackKind::Text);
    if (track == nullptr) {
        emit error("No title track");
        return {};
    }
    core::Clip title;
    title.id = core::IdGenerator::make("clip");
    title.name = "Title";
    title.sourceIn = core::Rational(0);
    title.sourceOut = core::Rational(3, 1);
    title.seqStart = trackEnd(*seq, *track);
    title.text = text.toStdString();
    title.fontFamily = "Arial";
    title.fontSizePt = 72.0;
    const QString clipId = QString::fromStdString(title.id);
    if (!execute(commands::makeAddClipCommand(track->id, std::move(title)))) {
        return {};
    }
    return clipId;
}

bool Session::splitSelectedAtPlayhead(const QString& clipId, double playheadSec) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr || clipId.isEmpty()) {
        return false;
    }
    const core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) {
        emit error("Clip not found");
        return false;
    }
    const core::Rational at = rationalFromSeconds(playheadSec);
    if (!(clip->seqStart < at && at < clip->seqEnd())) {
        emit error("Playhead is not inside the selected clip");
        return false;
    }
    return execute(commands::makeSplitClipCommand(
        clip->id, at, core::IdGenerator::make("clip")));
}

void Session::setRippleMode(bool v) {
    if (rippleMode_ != v) {
        rippleMode_ = v;
        emit rippleModeChanged();
    }
}

void Session::setSnappingEnabled(bool v) {
    if (snappingEnabled_ != v) {
        snappingEnabled_ = v;
        emit snappingEnabledChanged();
    }
}

double Session::snapTime(double targetSec, double thresholdSec) const {
    if (!snappingEnabled_) {
        return targetSec;
    }
    const core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        return targetSec;
    }
    double bestSnap = targetSec;
    double bestDist = thresholdSec;

    if (std::abs(targetSec) < bestDist) {
        bestDist = std::abs(targetSec);
        bestSnap = 0.0;
    }

    for (const auto& [id, clip] : seq->clips) {
        const double start = static_cast<double>(clip.seqStart);
        const double end = static_cast<double>(clip.seqEnd());
        const double distStart = std::abs(targetSec - start);
        if (distStart < bestDist) {
            bestDist = distStart;
            bestSnap = start;
        }
        const double distEnd = std::abs(targetSec - end);
        if (distEnd < bestDist) {
            bestDist = distEnd;
            bestSnap = end;
        }
    }
    return bestSnap;
}

bool Session::deleteClip(const QString& clipId) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr || clipId.isEmpty()) {
        return false;
    }
    for (const auto& track : seq->tracks) {
        for (const auto& id : track.clipIds) {
            if (id == clipId.toStdString()) {
                if (track.locked) {
                    emit error("Track is locked");
                    return false;
                }
                if (rippleMode_) {
                    return execute(commands::makeRippleDeleteClipCommand(track.id, id));
                }
                return execute(commands::makeRemoveClipCommand(track.id, id));
            }
        }
    }
    emit error("Clip is not on any track");
    return false;
}

bool Session::moveClipTo(const QString& clipId, double newStartSec) {
    if (clipId.isEmpty()) {
        return false;
    }
    return execute(commands::makeMoveClipCommand(clipId.toStdString(),
                                                 rationalFromSeconds(newStartSec)));
}

bool Session::moveClipToTrack(const QString& clipId, const QString& newTrackId, double newStartSec) {
    if (clipId.isEmpty() || newTrackId.isEmpty()) {
        return false;
    }
    return execute(commands::makeMoveClipToTrackCommand(clipId.toStdString(),
                                                        newTrackId.toStdString(),
                                                        rationalFromSeconds(newStartSec)));
}


bool Session::trimClip(const QString& clipId, double newInSec, double newOutSec,
                       double newStartSec) {
    if (clipId.isEmpty()) {
        return false;
    }
    if (rippleMode_) {
        return execute(commands::makeRippleTrimClipCommand(
            clipId.toStdString(), rationalFromSeconds(newInSec), rationalFromSeconds(newOutSec),
            rationalFromSeconds(newStartSec)));
    }
    return execute(commands::makeTrimClipCommand(
        clipId.toStdString(), rationalFromSeconds(newInSec), rationalFromSeconds(newOutSec),
        rationalFromSeconds(newStartSec)));
}

bool Session::setClipOpacity(const QString& clipId, double opacity) {
    if (clipId.isEmpty()) {
        return false;
    }
    return execute(commands::makeSetOpacityCommand(clipId.toStdString(), opacity));
}

bool Session::setClipSpeed(const QString& clipId, double speed) {
    if (clipId.isEmpty() || speed <= 0.0) {
        return false;
    }
    const auto micros = static_cast<std::int64_t>(std::llround(speed * 1000.0));
    const core::Rational ratSpeed(micros, 1000);
    return execute(commands::makeSetClipSpeedCommand(clipId.toStdString(), ratSpeed, rippleMode_));
}

bool Session::addClipEffect(const QString& clipId, const QString& effectType) {
    if (clipId.isEmpty() || effectType.isEmpty()) {
        return false;
    }
    core::Effect eff;
    eff.type = effectType.toStdString();
    eff.enabled = true;
    const auto* def = effects::EffectRegistry::defaults().find(eff.type);
    if (def != nullptr) {
        for (const auto& p : def->params) {
            eff.params[p.name] = p.def;
        }
    }
    return execute(commands::makeAddEffectCommand(clipId.toStdString(), std::move(eff)));
}

bool Session::removeClipEffect(const QString& clipId, int effectIndex) {
    if (clipId.isEmpty() || effectIndex < 0) {
        return false;
    }
    return execute(commands::makeRemoveEffectCommand(clipId.toStdString(), static_cast<size_t>(effectIndex)));
}

bool Session::updateClipEffectParam(const QString& clipId, int effectIndex,
                                    const QString& paramName, double paramValue) {
    if (clipId.isEmpty() || effectIndex < 0 || paramName.isEmpty()) {
        return false;
    }
    std::map<std::string, double> p;
    p[paramName.toStdString()] = paramValue;
    return execute(commands::makeUpdateEffectCommand(clipId.toStdString(), static_cast<size_t>(effectIndex),
                                                     std::move(p), {}));
}

bool Session::updateClipEffectStrParam(const QString& clipId, int effectIndex,
                                       const QString& paramName, const QString& paramValue) {
    if (clipId.isEmpty() || effectIndex < 0 || paramName.isEmpty()) {
        return false;
    }
    std::map<std::string, std::string> sp;
    sp[paramName.toStdString()] = paramValue.toStdString();
    return execute(commands::makeUpdateEffectCommand(clipId.toStdString(), static_cast<size_t>(effectIndex),
                                                     {}, std::move(sp)));
}

bool Session::setClipColorAdjust(const QString& clipId, double brightness,
                                 double contrast, double saturation,
                                 double temp, double tint) {
    if (clipId.isEmpty()) {
        return false;
    }
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) return false;

    // Check if color_adjust effect already exists on clip
    for (size_t i = 0; i < clip->effects.size(); ++i) {
        if (clip->effects[i].type == "color_adjust") {
            std::map<std::string, double> p;
            p["brightness"] = brightness;
            p["contrast"] = contrast;
            p["saturation"] = saturation;
            p["temperature"] = temp;
            p["tint"] = tint;
            return execute(commands::makeUpdateEffectCommand(clipId.toStdString(), i, std::move(p), {}));
        }
    }
    // If not found, add one!
    core::Effect eff;
    eff.type = "color_adjust";
    eff.enabled = true;
    eff.params["brightness"] = brightness;
    eff.params["contrast"] = contrast;
    eff.params["saturation"] = saturation;
    eff.params["temperature"] = temp;
    eff.params["tint"] = tint;
    return execute(commands::makeAddEffectCommand(clipId.toStdString(), std::move(eff)));
}

QString Session::addTransition(const QString& trackId, const QString& fromClipId,
                               const QString& toClipId, const QString& type,
                               double durationSec, int alignment) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        emit error("No active sequence");
        return {};
    }
    std::string tid = trackId.toStdString();
    if (tid.empty() && !fromClipId.isEmpty()) {
        for (const auto& t : seq->tracks) {
            for (const auto& cid : t.clipIds) {
                if (cid == fromClipId.toStdString()) {
                    tid = t.id;
                    break;
                }
            }
            if (!tid.empty()) break;
        }
    }
    if (tid.empty()) {
        emit error("Could not determine track for transition");
        return {};
    }
    core::Transition t;
    t.id = core::IdGenerator::make("trans");
    t.trackId = tid;
    t.fromClipId = fromClipId.toStdString();
    t.toClipId = toClipId.toStdString();
    t.type = type.isEmpty() ? "crossfade" : type.toStdString();
    t.duration = rationalFromSeconds(durationSec);
    if (alignment == 1) {
        t.alignment = core::TransitionAlignment::StartOnCut;
    } else if (alignment == 2) {
        t.alignment = core::TransitionAlignment::EndOnCut;
    } else {
        t.alignment = core::TransitionAlignment::CenterOnCut;
    }
    t.easing = "linear";

    const QString transId = QString::fromStdString(t.id);
    if (!execute(commands::makeAddTransitionCommand(std::move(t)))) {
        return {};
    }
    return transId;
}

bool Session::removeTransition(const QString& transitionId) {
    if (transitionId.isEmpty()) {
        return false;
    }
    return execute(commands::makeRemoveTransitionCommand(transitionId.toStdString()));
}

bool Session::updateTransition(const QString& transitionId, double durationSec,
                               int alignment, const QString& type, const QString& easing) {
    if (transitionId.isEmpty()) {
        return false;
    }
    core::TransitionAlignment align = core::TransitionAlignment::CenterOnCut;
    if (alignment == 1) {
        align = core::TransitionAlignment::StartOnCut;
    } else if (alignment == 2) {
        align = core::TransitionAlignment::EndOnCut;
    }

    return execute(commands::makeUpdateTransitionCommand(
        transitionId.toStdString(),
        rationalFromSeconds(durationSec),
        align,
        type.toStdString(),
        easing.toStdString()));
}

bool Session::setClipTransform(const QString& clipId, double scale, double x, double y,
                               double rotationDeg) {
    if (clipId.isEmpty()) {
        return false;
    }
    core::Transform tr;
    tr.scale = scale;
    tr.x = x;
    tr.y = y;
    tr.rotationDeg = rotationDeg;
    return execute(commands::makeSetTransformCommand(clipId.toStdString(), tr));
}

bool Session::setClipText(const QString& clipId, const QString& text,
                          const QString& fontFamily, double fontSizePt) {
    if (clipId.isEmpty()) {
        return false;
    }
    return execute(commands::makeSetTextCommand(clipId.toStdString(), text.toStdString(),
                                                fontFamily.toStdString(), fontSizePt));
}

void Session::undo() {
    if (undo_.undo(project_)) {
        dirty_ = true;
        emit projectChanged();
    }
}

void Session::redo() {
    std::string err;
    if (undo_.redo(project_, err)) {
        dirty_ = true;
        emit projectChanged();
    } else if (!err.empty()) {
        emit error(QString::fromStdString(err));
    }
}

bool Session::execute(std::unique_ptr<commands::ICommand> cmd) {
    std::string err;
    if (!undo_.execute(std::move(cmd), project_, err)) {
        emit error(QString::fromStdString(err));
        return false;
    }
    if (auto r = project_.validate(); r.isErr()) {
        // Defensive: commands validate preconditions, so this is unreachable
        // in practice — but never let the document go invalid silently.
        undo_.undo(project_);
        emit error("Edit rejected: " + QString::fromStdString(r.error()));
        return false;
    }
    dirty_ = true;
    emit projectChanged();
    return true;
}

void Session::ensureTimelineTracks(core::Sequence& seq) {
    const auto has = [&](core::TrackKind kind) {
        for (const auto& t : seq.tracks) {
            if (t.kind == kind) {
                return true;
            }
        }
        return false;
    };
    const auto add = [&](core::TrackKind kind, const char* name) {
        core::Track t;
        t.id = core::IdGenerator::make("track");
        t.kind = kind;
        t.name = name;
        seq.tracks.push_back(std::move(t));
    };
    if (!has(core::TrackKind::Video)) {
        add(core::TrackKind::Video, "V1");
    }
    if (!has(core::TrackKind::Audio)) {
        add(core::TrackKind::Audio, "A1");
    }
    if (!has(core::TrackKind::Text)) {
        add(core::TrackKind::Text, "T1");
    }
}

core::Track* Session::findTrackForKind(core::Sequence& seq, core::TrackKind kind) {
    for (auto& t : seq.tracks) {
        if (t.kind == kind) {
            return &t;
        }
    }
    return nullptr;
}

core::Rational Session::trackEnd(const core::Sequence& seq, const core::Track& track) const {
    core::Rational end{0};
    for (const auto& id : track.clipIds) {
        const auto it = seq.clips.find(id);
        if (it != seq.clips.end() && end < it->second.seqEnd()) {
            end = it->second.seqEnd();
        }
    }
    return end;
}

bool Session::autoSilenceCut(const QString& clipId, double thresholdDb, double minDurationSec) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    const std::string cid = clipId.toStdString();
    core::Clip* clip = seq->findClip(cid);
    if (clip == nullptr) return false;

    core::Track* track = nullptr;
    for (auto& t : seq->tracks) {
        if (std::find(t.clipIds.begin(), t.clipIds.end(), cid) != t.clipIds.end()) {
            track = &t;
            break;
        }
    }
    if (track == nullptr) return false;

    auto assetIt = project_.assets.find(clip->assetId);
    if (assetIt == project_.assets.end()) return false;

    media_ffmpeg::AudioDecoder dec;
    if (dec.open(assetIt->second.path).isErr()) return false;
    if (!dec.hasAudio()) return false;

    if (dec.seek(clip->sourceIn).isErr()) return false;

    std::vector<float> pcmFloat;
    const core::Rational spanDur = clip->sourceOut - clip->sourceIn;
    const int targetSamples = static_cast<int>(std::ceil(static_cast<double>(spanDur) * 48000.0)) * 2;

    while (static_cast<int>(pcmFloat.size()) < targetSamples) {
        auto chunkRes = dec.nextChunk(4096);
        if (chunkRes.isErr()) break;
        const auto& chunk = chunkRes.value();
        if (chunk.pcm.empty()) break;
        for (std::int16_t s : chunk.pcm) {
            pcmFloat.push_back(static_cast<float>(s) / 32768.0f);
        }
    }

    if (pcmFloat.empty()) return false;

    auto silences = ai::AudioAnalysis::detectSilences(
        pcmFloat.data(), pcmFloat.size(), 48000, 2, thresholdDb, minDurationSec
    );

    if (silences.empty()) return true;

    for (auto& s : silences) {
        s.startSec = clip->seqStart + (s.startSec / clip->speed);
        s.durationSec = s.durationSec / clip->speed;
    }

    return execute(commands::makeSilenceCutCommand(track->id, cid, silences));
}

bool Session::autoSceneSplit(const QString& clipId, double threshold) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    const std::string cid = clipId.toStdString();
    core::Clip* clip = seq->findClip(cid);
    if (clip == nullptr) return false;

    auto assetIt = project_.assets.find(clip->assetId);
    if (assetIt == project_.assets.end()) return false;

    media_ffmpeg::VideoDecoder dec;
    if (dec.open(assetIt->second.path).isErr()) return false;

    const core::Rational step(1, 5); // 5 fps sampling
    std::vector<core::Rational> sampleTimes;
    std::vector<double> scores;

    std::vector<uint8_t> prevRgba;
    core::Rational t = clip->sourceIn;
    while (t < clip->sourceOut) {
        if (dec.seek(t).isOk()) {
            auto frameRes = dec.nextFrame();
            if (frameRes.isOk()) {
                const auto& vf = frameRes.value();
                if (!prevRgba.empty() && vf.width > 0 && vf.height > 0) {
                    double diff = ai::SceneDetection::calculateFrameDifference(
                        prevRgba.data(), vf.rgba.data(), vf.width, vf.height, vf.width * 4
                    );
                    sampleTimes.push_back(t);
                    scores.push_back(diff);
                }
                prevRgba = vf.rgba;
            }
        }
        t = t + step;
    }

    auto cuts = ai::SceneDetection::findCutsFromScores(sampleTimes, scores, threshold);
    if (cuts.empty()) return true;

    std::vector<core::Rational> seqCutPoints;
    for (const auto& c : cuts) {
        core::Rational seqPt = clip->seqStart + (c.timestamp - clip->sourceIn) / clip->speed;
        seqCutPoints.push_back(seqPt);
    }

    return execute(commands::makeSceneSplitCommand(cid, seqCutPoints));
}

bool Session::generateAutoCaptions(const QString& transcript, int wordsPerCue) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    ensureTimelineTracks(*seq);
    core::Track* textTrack = findTrackForKind(*seq, core::TrackKind::Text);
    if (textTrack == nullptr) return false;

    core::Rational totalDur = trackEnd(*seq, *textTrack);
    if (totalDur.num() <= 0) {
        for (const auto& t : seq->tracks) {
            core::Rational e = trackEnd(*seq, t);
            if (e > totalDur) totalDur = e;
        }
    }
    if (totalDur.num() <= 0) totalDur = core::Rational(10, 1);

    auto cues = ai::CaptionEngine::chunkTranscript(transcript.toStdString(), totalDur, static_cast<std::size_t>(wordsPerCue));
    if (cues.empty()) return false;

    return execute(commands::makeAddCaptionsCommand(textTrack->id, cues));
}

bool Session::importSubtitles(const QString& srtContent) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    ensureTimelineTracks(*seq);
    core::Track* textTrack = findTrackForKind(*seq, core::TrackKind::Text);
    if (textTrack == nullptr) return false;

    auto cues = ai::CaptionEngine::parseSrt(srtContent.toStdString());
    if (cues.empty()) return false;

    return execute(commands::makeAddCaptionsCommand(textTrack->id, cues));
}

bool Session::importSubtitlesFile(const QUrl& url) {
    const QString local = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile f(local);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error("Failed to open subtitle file: " + local);
        return false;
    }
    return importSubtitles(QString::fromUtf8(f.readAll()));
}

QString Session::exportSubtitles() const {
    const core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return "";
    const core::Track* textTrack = nullptr;
    for (const auto& t : seq->tracks) {
        if (t.kind == core::TrackKind::Text) {
            textTrack = &t;
            break;
        }
    }
    if (textTrack == nullptr) return "";

    std::vector<ai::CaptionCue> cues;
    for (const auto& cid : textTrack->clipIds) {
        auto it = seq->clips.find(cid);
        if (it != seq->clips.end()) {
            cues.push_back(ai::CaptionCue{
                it->second.seqStart,
                it->second.seqDuration(),
                it->second.text
            });
        }
    }
    std::sort(cues.begin(), cues.end(), [](const auto& a, const auto& b) {
        return a.startSec < b.startSec;
    });
    return QString::fromStdString(ai::CaptionEngine::exportSrt(cues));
}

bool Session::exportSubtitlesFile(const QUrl& url) const {
    const QString local = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile f(local);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    const QString srt = exportSubtitles();
    f.write(srt.toUtf8());
    return true;
}

bool Session::setClipKeyframe(const QString& clipId, double seqTimeSec,
                             double scale, double x, double y,
                             double rotationDeg, double opacity,
                             const QString& easing) {
    if (clipId.isEmpty()) return false;
    core::Keyframe kf;
    kf.seqTime = rationalFromSeconds(seqTimeSec);
    kf.transform.scale = scale;
    kf.transform.x = x;
    kf.transform.y = y;
    kf.transform.rotationDeg = rotationDeg;
    kf.opacity = (std::max)(0.0, (std::min)(1.0, opacity));
    kf.easing = easing.isEmpty() ? "linear" : easing.toStdString();
    return execute(commands::makeSetKeyframeCommand(clipId.toStdString(), std::move(kf)));
}

bool Session::removeClipKeyframe(const QString& clipId, double seqTimeSec) {
    if (clipId.isEmpty()) return false;
    return execute(commands::makeRemoveKeyframeCommand(clipId.toStdString(), rationalFromSeconds(seqTimeSec)));
}

bool Session::setClipFade(const QString& clipId, double fadeInSec, double fadeOutSec) {
    if (clipId.isEmpty()) return false;
    return execute(commands::makeSetFadeCommand(clipId.toStdString(), fadeInSec, fadeOutSec));
}

bool Session::setSequenceFormat(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    return execute(commands::makeSetSequenceFormatCommand(width, height));
}

bool Session::setSequenceAspectPreset(const QString& preset) {
    if (preset == "16:9") {
        return setSequenceFormat(1920, 1080);
    } else if (preset == "9:16") {
        return setSequenceFormat(1080, 1920);
    } else if (preset == "1:1") {
        return setSequenceFormat(1080, 1080);
    } else if (preset == "4:5") {
        return setSequenceFormat(1080, 1350);
    } else if (preset == "21:9") {
        return setSequenceFormat(2560, 1080);
    }
    return false;
}

QString Session::getSequenceAspectPreset() const {
    const core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return "16:9";
    if (seq->width == 1920 && seq->height == 1080) return "16:9";
    if (seq->width == 1080 && seq->height == 1920) return "9:16";
    if (seq->width == 1080 && seq->height == 1080) return "1:1";
    if (seq->width == 1080 && seq->height == 1350) return "4:5";
    if (seq->width == 2560 && seq->height == 1080) return "21:9";
    return QString::number(seq->width) + "x" + QString::number(seq->height);
}

bool Session::normalizeClipAudio(const QString& clipId, double targetLufs) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) return false;

    auto assetIt = project_.assets.find(clip->assetId);
    if (assetIt == project_.assets.end()) return false;

    media_ffmpeg::AudioDecoder dec;
    if (dec.open(assetIt->second.path).isErr() || !dec.hasAudio()) return false;
    if (dec.seek(clip->sourceIn).isErr()) return false;

    std::vector<float> pcmFloat;
    const core::Rational spanDur = clip->sourceOut - clip->sourceIn;
    const int targetSamples = static_cast<int>(std::ceil(static_cast<double>(spanDur) * 48000.0)) * 2;

    while (static_cast<int>(pcmFloat.size()) < targetSamples) {
        auto chunkRes = dec.nextChunk(4096);
        if (chunkRes.isErr()) break;
        const auto& chunk = chunkRes.value();
        if (chunk.pcm.empty()) break;
        for (std::int16_t s : chunk.pcm) {
            pcmFloat.push_back(static_cast<float>(s) / 32768.0f);
        }
    }
    if (pcmFloat.empty()) return false;

    double curLufs = ai::AudioAnalysis::calculateIntegratedLufs(pcmFloat.data(), pcmFloat.size(), 48000, 2);
    if (curLufs <= -60.0) return true;

    double deltaDb = targetLufs - curLufs;
    double gain = std::pow(10.0, deltaDb / 20.0);
    double newOpacity = (std::max)(0.05, (std::min)(2.0, clip->opacity * gain));
    return execute(commands::makeSetOpacityCommand(clip->id, newOpacity));
}

bool Session::denoiseClipAudio(const QString& clipId, double rumbleCutoffHz) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) return false;

    core::Effect eff;
    eff.type = "voice_denoise";
    eff.enabled = true;
    eff.params["rumble_cutoff_hz"] = rumbleCutoffHz;
    eff.params["intensity"] = 0.7;
    return execute(commands::makeAddEffectCommand(clip->id, std::move(eff)));
}

bool Session::clipHasAudio(const QString& clipId) const {
    const core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr || clipId.isEmpty()) return false;
    const core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) return false;
    const auto ait = project_.assets.find(clip->assetId);
    if (ait == project_.assets.end()) return false;
    return ait->second.hasAudio;
}

bool Session::extractAudioFromClip(const QString& clipId) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        emit error("No active sequence");
        return false;
    }
    core::Clip* vClip = seq->findClip(clipId.toStdString());
    if (vClip == nullptr) {
        emit error("Clip not found");
        return false;
    }
    const auto ait = project_.assets.find(vClip->assetId);
    if (ait == project_.assets.end() || !ait->second.hasAudio) {
        emit error("Selected clip has no audio to extract");
        return false;
    }

    ensureTimelineTracks(*seq);

    // Find an audio track that doesn't collide with this clip's sequence interval
    core::Track* targetTrack = nullptr;
    const core::TimeRange clipRange = vClip->seqRange();

    for (auto& track : seq->tracks) {
        if (track.kind != core::TrackKind::Audio) {
            continue;
        }
        bool collides = false;
        for (const auto& cid : track.clipIds) {
            if (const core::Clip* existing = seq->findClip(cid)) {
                if (existing->seqRange().overlaps(clipRange)) {
                    collides = true;
                    break;
                }
            }
        }
        if (!collides) {
            targetTrack = &track;
            break;
        }
    }

    if (targetTrack == nullptr) {
        core::Track newTrack;
        newTrack.id = core::IdGenerator::make("track");
        newTrack.kind = core::TrackKind::Audio;
        int aCount = 0;
        for (const auto& t : seq->tracks) {
            if (t.kind == core::TrackKind::Audio) aCount++;
        }
        newTrack.name = "A" + std::to_string(aCount + 1);
        seq->tracks.push_back(std::move(newTrack));
        targetTrack = &seq->tracks.back();
    }

    core::Clip aClip;
    aClip.id = core::IdGenerator::make("clip");
    aClip.assetId = vClip->assetId;
    aClip.name = vClip->name + " (Audio)";
    aClip.sourceIn = vClip->sourceIn;
    aClip.sourceOut = vClip->sourceOut;
    aClip.seqStart = vClip->seqStart;
    aClip.speed = vClip->speed;
    aClip.opacity = 1.0;
    aClip.fadeInSec = vClip->fadeInSec;
    aClip.fadeOutSec = vClip->fadeOutSec;

    const std::string trkId = targetTrack->id;
    if (!execute(commands::makeAddClipCommand(trkId, std::move(aClip)))) {
        emit error("Failed to add extracted audio clip to timeline");
        return false;
    }
    dirty_ = true;
    emit projectChanged();
    return true;
}

QString Session::extractAudioToFile(const QString& clipId, const QString& destinationPath) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) {
        emit error("No active sequence");
        return {};
    }
    core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) {
        emit error("Clip not found");
        return {};
    }
    const auto ait = project_.assets.find(clip->assetId);
    if (ait == project_.assets.end() || !ait->second.hasAudio) {
        emit error("Selected clip has no audio to extract");
        return {};
    }

    QString outPath = destinationPath;
    if (outPath.isEmpty()) {
        QString baseDir = filePath_.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                              : QFileInfo(filePath_).absolutePath();
        QString baseName = QString::fromStdString(clip->name).trimmed();
        if (baseName.isEmpty()) baseName = "extracted_audio";
        outPath = baseDir + "/" + baseName + "_audio.wav";
    }

    media_ffmpeg::AudioDecoder dec;
    if (dec.open(ait->second.path).isErr() || !dec.hasAudio()) {
        emit error("Failed to open audio decoder for asset");
        return {};
    }
    if (dec.seek(clip->sourceIn).isErr()) {
        emit error("Failed to seek to clip start in audio");
        return {};
    }

    QFile wavFile(outPath);
    if (!wavFile.open(QIODevice::WriteOnly)) {
        emit error("Cannot open destination file for writing: " + outPath);
        return {};
    }

    // Write placeholder WAV header
    const uint32_t sampleRate = 48000;
    const uint16_t numChannels = 2;
    const uint16_t bitsPerSample = 16;
    const uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    const uint16_t blockAlign = numChannels * (bitsPerSample / 8);

    wavFile.write("RIFF", 4);
    uint32_t zero32 = 0;
    wavFile.write(reinterpret_cast<const char*>(&zero32), 4);
    wavFile.write("WAVEfmt ", 8);
    uint32_t fmtSize = 16;
    wavFile.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t audioFormat = 1; // PCM
    wavFile.write(reinterpret_cast<const char*>(&audioFormat), 2);
    wavFile.write(reinterpret_cast<const char*>(&numChannels), 2);
    wavFile.write(reinterpret_cast<const char*>(&sampleRate), 4);
    wavFile.write(reinterpret_cast<const char*>(&byteRate), 4);
    wavFile.write(reinterpret_cast<const char*>(&blockAlign), 2);
    wavFile.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    wavFile.write("data", 4);
    wavFile.write(reinterpret_cast<const char*>(&zero32), 4);

    uint32_t totalPcmBytes = 0;
    const core::Rational spanDur = clip->sourceOut - clip->sourceIn;
    const int64_t maxSamplesPerChannel =
        static_cast<int64_t>(std::ceil(static_cast<double>(spanDur) * static_cast<double>(sampleRate)));
    int64_t samplesRead = 0;

    while (samplesRead < maxSamplesPerChannel) {
        int need = static_cast<int>(std::min<int64_t>(4096, maxSamplesPerChannel - samplesRead));
        auto chunkRes = dec.nextChunk(need);
        if (chunkRes.isErr() || chunkRes.value().pcm.empty()) {
            break;
        }
        const auto& pcm = chunkRes.value().pcm;
        const qint64 bytesToWrite = static_cast<qint64>(pcm.size() * sizeof(std::int16_t));
        wavFile.write(reinterpret_cast<const char*>(pcm.data()), bytesToWrite);
        totalPcmBytes += static_cast<uint32_t>(bytesToWrite);
        samplesRead += pcm.size() / numChannels;
    }

    const uint32_t riffSize = 36 + totalPcmBytes;
    wavFile.seek(4);
    wavFile.write(reinterpret_cast<const char*>(&riffSize), 4);
    wavFile.seek(40);
    wavFile.write(reinterpret_cast<const char*>(&totalPcmBytes), 4);
    wavFile.close();

    // Register into project media library so it appears immediately in Media Browser
    importMedia(QUrl::fromLocalFile(outPath));

    return outPath;
}

bool Session::setTrackMuted(const QString& trackId, bool muted) {
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr || trackId.isEmpty()) return false;
    for (auto& t : seq->tracks) {
        if (t.id == trackId.toStdString() || t.name == trackId.toStdString()) {
            if (t.muted == muted) return true;
            t.muted = muted;
            dirty_ = true;
            emit projectChanged();
            return true;
        }
    }
    return false;
}

bool Session::setClipBlendMode(const QString& clipId, const QString& mode) {
    if (clipId.isEmpty()) return false;
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) return false;

    for (size_t i = 0; i < clip->effects.size(); ++i) {
        if (clip->effects[i].type == "blend_mode") {
            std::map<std::string, std::string> sp;
            sp["mode"] = mode.toLower().toStdString();
            return execute(commands::makeUpdateEffectCommand(clipId.toStdString(), i, {}, std::move(sp)));
        }
    }
    core::Effect eff;
    eff.type = "blend_mode";
    eff.enabled = true;
    eff.strParams["mode"] = mode.toLower().toStdString();
    return execute(commands::makeAddEffectCommand(clipId.toStdString(), std::move(eff)));
}

bool Session::setClipHighlightsShadows(const QString& clipId, double highlights, double shadows) {
    if (clipId.isEmpty()) return false;
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) return false;

    for (size_t i = 0; i < clip->effects.size(); ++i) {
        if (clip->effects[i].type == "highlights_shadows") {
            std::map<std::string, double> p;
            p["highlights"] = highlights;
            p["shadows"] = shadows;
            return execute(commands::makeUpdateEffectCommand(clipId.toStdString(), i, std::move(p), {}));
        }
    }
    core::Effect eff;
    eff.type = "highlights_shadows";
    eff.enabled = true;
    eff.params["highlights"] = highlights;
    eff.params["shadows"] = shadows;
    return execute(commands::makeAddEffectCommand(clipId.toStdString(), std::move(eff)));
}

bool Session::setClipColorWheels(const QString& clipId, double liftY, double gammaY, double gainY,
                                double offsetR, double offsetG, double offsetB, double lumaMix) {
    if (clipId.isEmpty()) return false;
    core::Sequence* seq = project_.activeSequence();
    if (seq == nullptr) return false;
    core::Clip* clip = seq->findClip(clipId.toStdString());
    if (clip == nullptr) return false;

    for (size_t i = 0; i < clip->effects.size(); ++i) {
        if (clip->effects[i].type == "color_wheels") {
            std::map<std::string, double> p;
            p["liftY"] = liftY;
            p["gammaY"] = gammaY;
            p["gainY"] = gainY;
            p["offsetR"] = offsetR;
            p["offsetG"] = offsetG;
            p["offsetB"] = offsetB;
            p["lumaMix"] = lumaMix;
            return execute(commands::makeUpdateEffectCommand(clipId.toStdString(), i, std::move(p), {}));
        }
    }
    core::Effect eff;
    eff.type = "color_wheels";
    eff.enabled = true;
    eff.params["liftY"] = liftY;
    eff.params["gammaY"] = gammaY;
    eff.params["gainY"] = gainY;
    eff.params["offsetR"] = offsetR;
    eff.params["offsetG"] = offsetG;
    eff.params["offsetB"] = offsetB;
    eff.params["lumaMix"] = lumaMix;
    return execute(commands::makeAddEffectCommand(clipId.toStdString(), std::move(eff)));
}

bool Session::importCapCutDraft(const QUrl& url) {
    QString local = url.isLocalFile() ? url.toLocalFile() : url.toString();
    auto res = persist::CapCutDraftBridge::importDraft(local.toStdString());
    if (res.isErr()) {
        emit error(QString::fromStdString(res.error()));
        return false;
    }
    project_ = res.value();
    undo_.clear();
    filePath_.clear();
    dirty_ = false;
    emit projectChanged();
    return true;
}

bool Session::exportCapCutDraft(const QUrl& url) {
    QString local = url.isLocalFile() ? url.toLocalFile() : url.toString();
    auto res = persist::CapCutDraftBridge::exportDraft(project_, local.toStdString());
    if (res.isErr()) {
        emit error(QString::fromStdString(res.error()));
        return false;
    }
    return true;
}

bool Session::relinkAsset(const QString& assetId, const QString& newPath) {
    auto it = project_.assets.find(assetId.toStdString());
    if (it == project_.assets.end()) return false;
    QString clean = QUrl(newPath).isLocalFile() ? QUrl(newPath).toLocalFile() : newPath;
    if (!QFile::exists(clean)) {
        emit error("Selected file does not exist: " + clean);
        return false;
    }
    it->second.path = clean.toStdString();
    dirty_ = true;
    emit projectChanged();
    return true;
}

int Session::sequenceWidth() const {
    const auto* seq = project_.activeSequence();
    return seq ? static_cast<int>(seq->width) : 1920;
}

int Session::sequenceHeight() const {
    const auto* seq = project_.activeSequence();
    return seq ? static_cast<int>(seq->height) : 1080;
}

double Session::sequenceFps() const {
    const auto* seq = project_.activeSequence();
    return seq ? static_cast<double>(seq->fps) : 30.0;
}

} // namespace editor::shell
