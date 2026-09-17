#include "TestHarness.hpp"

#include "shell/ExportController.hpp"
#include "shell/Player.hpp"
#include "shell/Session.hpp"
#include "shell/ThumbnailProvider.hpp"
#include "shell/TimelineModel.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include <atomic>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

using namespace editor;

namespace {

QCoreApplication* ensureApp() {
    static int argc = 1;
    static char appName[] = "test_shell";
    static char* argv[] = {appName, nullptr};
    if (QCoreApplication::instance() == nullptr) {
        new QCoreApplication(argc, argv);
    }
    return QCoreApplication::instance();
}

bool waitForState(shell::ExportController& ec, int targetState, int maxWaitMs = 15000) {
    QElapsedTimer timer;
    timer.start();
    while (!timer.hasExpired(maxWaitMs)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (ec.state() == targetState) {
            return true;
        }
        QThread::msleep(10);
    }
    return (ec.state() == targetState);
}

void writeWavFile(const QString& path, int sampleRate, int numChannels, const std::vector<int16_t>& pcm) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return;
    uint32_t dataSize = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    uint32_t fileSize = 36 + dataSize;
    f.write("RIFF", 4);
    f.write(reinterpret_cast<const char*>(&fileSize), 4);
    f.write("WAVEfmt ", 8);
    uint32_t fmtSize = 16;
    f.write(reinterpret_cast<const char*>(&fmtSize), 4);
    uint16_t fmt = 1; // PCM
    f.write(reinterpret_cast<const char*>(&fmt), 2);
    uint16_t channels = static_cast<uint16_t>(numChannels);
    f.write(reinterpret_cast<const char*>(&channels), 2);
    uint32_t sRate = static_cast<uint32_t>(sampleRate);
    f.write(reinterpret_cast<const char*>(&sRate), 4);
    uint32_t byteRate = sRate * channels * 2;
    f.write(reinterpret_cast<const char*>(&byteRate), 4);
    uint16_t blockAlign = channels * 2;
    f.write(reinterpret_cast<const char*>(&blockAlign), 2);
    uint16_t bitsPerSample = 16;
    f.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    f.write("data", 4);
    f.write(reinterpret_cast<const char*>(&dataSize), 4);
    f.write(reinterpret_cast<const char*>(pcm.data()), dataSize);
    f.close();
}

} // namespace

TEST_CASE("shell: Session saveAs commits path only on success, preserving dirty and original path on failure") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    shell::Session session;
    session.newProject();
    session.addTitle("Title 1"); // marks dirty
    CHECK(session.dirty());
    CHECK(session.filePath().isEmpty());

    // 1. Try to save to an invalid path
    QUrl invalidUrl = QUrl::fromLocalFile("Z:/nonexistent_drive_98765/invalid/path/project.json");
    bool ok = session.saveAs(invalidUrl);
    CHECK(!ok);
    CHECK(session.filePath().isEmpty()); // MUST NOT adopt the failed destination!
    CHECK(session.dirty());              // MUST NOT clear dirty state!

    // 2. Save to a valid destination
    QString validPath = tempDir.filePath("my_project.json");
    QUrl validUrl = QUrl::fromLocalFile(validPath);
    ok = session.saveAs(validUrl);
    CHECK(ok);
    CHECK_EQ(session.filePath().toStdString(), validPath.toStdString());
    CHECK(!session.dirty());

    // 3. Make another edit, then attempt a failed saveAs
    session.addTitle("Title 2");
    CHECK(session.dirty());
    ok = session.saveAs(invalidUrl);
    CHECK(!ok);
    // Path MUST still be the previous valid path, NOT the failed destination!
    CHECK_EQ(session.filePath().toStdString(), validPath.toStdString());
    CHECK(session.dirty());
}

TEST_CASE("shell: Session recovery storage preserves original filePath and dirty state") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    shell::Session session;
    session.setRecoveryDirectoryForTesting(tempDir.path());
    session.newProject();
    QString origPath = tempDir.filePath("original_project.json");
    CHECK(session.saveAs(QUrl::fromLocalFile(origPath)));
    CHECK(!session.dirty());
    CHECK_EQ(session.filePath().toStdString(), origPath.toStdString());

    // Add edit
    session.addTitle("Recovery Title");
    CHECK(session.dirty());

    // Save recovery snapshot
    session.clearRecovery();
    CHECK(!session.hasRecovery());
    CHECK(session.saveRecovery());
    CHECK(session.hasRecovery());

    // Autosave recovery MUST NOT touch filePath or dirty
    CHECK(session.dirty());
    CHECK_EQ(session.filePath().toStdString(), origPath.toStdString());

    QVariantMap recInfo = session.recoveryInfo();
    CHECK(recInfo["hasRecovery"].toBool());
    CHECK_EQ(recInfo["originalFilePath"].toString().toStdString(), origPath.toStdString());
    CHECK(!recInfo["timestamp"].toString().isEmpty());

    // Now simulate reopening from recovery
    shell::Session restoredSession;
    restoredSession.setRecoveryDirectoryForTesting(tempDir.path());
    restoredSession.newProject();
    CHECK(restoredSession.restoreRecovery());
    CHECK(restoredSession.dirty()); // Recovery must restore as DIRTY work!
    CHECK_EQ(restoredSession.filePath().toStdString(), origPath.toStdString());

    // Discard recovery
    session.discardRecovery();
    CHECK(!session.hasRecovery());
}

TEST_CASE("shell: Session recovery discard on Don't Save and corrupted/interrupted metadata integrity") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    shell::Session session;
    session.setRecoveryDirectoryForTesting(tempDir.path());
    session.newProject();
    session.addTitle("Recovery Integrity Test");

    // 1. Clean discard on Don't Save
    session.saveRecovery();
    CHECK(session.hasRecovery());
    session.discardRecovery();
    CHECK(!session.hasRecovery());

    // Next session starts clean
    shell::Session nextSession;
    nextSession.setRecoveryDirectoryForTesting(tempDir.path());
    CHECK(!nextSession.hasRecovery());

    // 2. Interrupted/corrupted metadata write: contentHash mismatch
    session.saveRecovery();
    CHECK(session.hasRecovery());

    // Corrupt metadata contentHash in the active recovery envelope
    QString ptrPath = tempDir.filePath("current.ptr");
    CHECK(QFile::exists(ptrPath));
    QFile pf(ptrPath);
    CHECK(pf.open(QIODevice::ReadOnly));
    QString activeGen = QString::fromUtf8(pf.readAll().trimmed());
    pf.close();
    QString activeGenPath = tempDir.filePath(activeGen);
    CHECK(QFile::exists(activeGenPath));

    {
        QFile f(activeGenPath);
        CHECK(f.open(QIODevice::WriteOnly));
        f.write("{\"generation\":1,\"projectName\":\"Fake\",\"originalFilePath\":\"wrong.json\",\"contentHash\":\"bad_hash\",\"project\":{}}");
        f.close();
    }

    // Must be rejected as invalid/corrupted recovery!
    CHECK(!session.hasRecovery());
    shell::Session badRestore;
    badRestore.setRecoveryDirectoryForTesting(tempDir.path());
    CHECK(!badRestore.restoreRecovery()); // Must refuse to restore with stale/corrupted metadata!

    // Cleanup
    session.clearRecovery();
}

TEST_CASE("shell: ExportController destination validation protects project files via filesystem identity, case variants, and aliases") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    shell::Session session;
    session.newProject();
    QString projPath = tempDir.filePath("MyProject.json");
    CHECK(session.saveAs(QUrl::fromLocalFile(projPath)));

    // Create dummy media file
    QString mediaPath = tempDir.filePath("source_asset.mp4");
    {
        QFile f(mediaPath);
        f.open(QIODevice::WriteOnly);
        f.write("data");
        f.close();
    }
    core::Asset asset;
    asset.id = "asset-test";
    asset.path = mediaPath.toStdString();
    asset.duration = core::Rational(10, 1);
    session.project().assets.emplace(asset.id, asset);

    shell::ExportController exporter;
    exporter.setSession(&session);

    // 1. Exact project path
    CHECK(exporter.validateDestination(projPath).contains("project file"));

    // 2. Windows case variants (MYPROJECT.JSON, myproject.json)
    QString upperProj = tempDir.filePath("MYPROJECT.JSON");
    QString lowerProj = tempDir.filePath("myproject.json");
    CHECK(exporter.validateDestination(upperProj).contains("project file"));
    CHECK(exporter.validateDestination(lowerProj).contains("project file"));

    // 3. Relative aliases (./MyProject.json, subdir/../MyProject.json)
    QString dotProj = tempDir.path() + "/./MyProject.json";
    QString upProj = tempDir.path() + "/dummy_sub/../MyProject.json";
    CHECK(exporter.validateDestination(dotProj).contains("project file"));
    CHECK(exporter.validateDestination(upProj).contains("project file"));

    // 4. Source media asset case variants and aliases
    QString upperMedia = tempDir.filePath("SOURCE_ASSET.MP4");
    QString dotMedia = tempDir.path() + "/./source_asset.mp4";
    CHECK(exporter.validateDestination(upperMedia).contains("source asset"));
    CHECK(exporter.validateDestination(dotMedia).contains("source asset"));

    // 5. Existing non-conflicting file
    QString other = tempDir.filePath("other_existing.mp4");
    {
        QFile f(other);
        f.open(QIODevice::WriteOnly);
        f.write("abc");
        f.close();
    }
    CHECK_EQ(exporter.validateDestination(other).toStdString(), "EXISTS");

    // 6. Valid new path
    QString cleanPath = tempDir.filePath("brand_new.mp4");
    CHECK_EQ(exporter.validateDestination(cleanPath).toStdString(), "");
}

TEST_CASE("shell: ExportController keeps completed export destination tied to job despite mid-export path edits") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    shell::Session session;
    session.newProject();
    core::Sequence* seq = session.project().activeSequence();
    CHECK(seq != nullptr);

    core::Asset asset;
    asset.id = "asset-fix";
    asset.path = FIXTURE_MP4;
    asset.duration = core::Rational(2, 1);
    session.project().assets[asset.id] = asset;

    core::Clip clip;
    clip.id = "clip-fix";
    clip.assetId = "asset-fix";
    clip.sourceIn = core::Rational(0);
    clip.sourceOut = core::Rational(1, 1);
    clip.seqStart = core::Rational(0);
    seq->clips[clip.id] = clip;
    seq->tracks.front().clipIds.push_back(clip.id);

    shell::ExportController exporter;
    exporter.setSession(&session);

    QString initialDest = tempDir.filePath("actual_job_output.mp4");
    QString editedDest = tempDir.filePath("user_edited_later.mp4");

    exporter.setOutputPath(initialDest);
    exporter.startExport();
    CHECK_EQ(exporter.state(), shell::ExportController::Running);

    // Edit requested output path while export is actively executing!
    exporter.setOutputPath(editedDest);
    CHECK_EQ(exporter.outputPath().toStdString(), editedDest.toStdString());

    // Wait for export to finish
    bool ok = waitForState(exporter, shell::ExportController::Done, 30000);
    CHECK(ok);

    // Completed destination MUST remain tied to the job's initial captured output, NOT the mid-job edited path!
    QFileInfo compFi(exporter.completedOutputPath());
    QFileInfo initFi(initialDest);
    CHECK_EQ(compFi.absoluteFilePath().toStdString(), initFi.absoluteFilePath().toStdString());
    CHECK(compFi.exists());
    CHECK(!QFile::exists(editedDest));
}

TEST_CASE("shell: ExportController asynchronous rapid repeat, cancel, failure, and destruction lifecycle") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    shell::Session session;
    session.newProject();
    core::Sequence* seq = session.project().activeSequence();
    CHECK(seq != nullptr);

    core::Asset asset;
    asset.id = "asset-fix";
    asset.path = FIXTURE_MP4;
    asset.duration = core::Rational(2, 1);
    session.project().assets[asset.id] = asset;

    core::Clip clip;
    clip.id = "clip-fix";
    clip.assetId = "asset-fix";
    clip.sourceIn = core::Rational(0);
    clip.sourceOut = core::Rational(1, 1);
    seq->clips[clip.id] = clip;
    seq->tracks.front().clipIds.push_back(clip.id);

    shell::ExportController exporter;
    exporter.setSession(&session);

    // 1. Cancel and retry
    exporter.setOutputPath(tempDir.filePath("cancel_test.mp4"));
    exporter.startExport();
    CHECK_EQ(exporter.state(), shell::ExportController::Running);
    exporter.cancelExport();
    bool cancelled = waitForState(exporter, shell::ExportController::Cancelled, 10000);
    CHECK(cancelled);

    exporter.reset();
    CHECK_EQ(exporter.state(), shell::ExportController::Idle);
    exporter.startExport();
    CHECK_EQ(exporter.state(), shell::ExportController::Running);
    exporter.cancelExport();
    waitForState(exporter, shell::ExportController::Cancelled, 10000);

    // 2. Failure and retry
    exporter.reset();
    exporter.setOutputPath(""); // Empty path triggers failure
    exporter.startExport();
    CHECK_EQ(exporter.state(), shell::ExportController::Failed);

    exporter.reset();
    exporter.setOutputPath(tempDir.filePath("retry_test.mp4"));
    exporter.startExport();
    CHECK_EQ(exporter.state(), shell::ExportController::Running);
    exporter.cancelExport();
    waitForState(exporter, shell::ExportController::Cancelled, 10000);

    // 3. Rapid repeat export without crash
    for (int i = 0; i < 3; ++i) {
        exporter.reset();
        exporter.setOutputPath(tempDir.filePath(QString("rapid_%1.mp4").arg(i)));
        exporter.startExport();
        exporter.cancelExport();
        waitForState(exporter, shell::ExportController::Cancelled, 10000);
    }

    // 4. Destruction while export worker is actively running
    {
        auto dynExporter = std::make_unique<shell::ExportController>();
        dynExporter->setSession(&session);
        dynExporter->setOutputPath(tempDir.filePath("destruct_run.mp4"));
        dynExporter->startExport();
        CHECK_EQ(dynExporter->state(), shell::ExportController::Running);
        // Destroy while running!
        dynExporter.reset();
        CHECK(dynExporter == nullptr);
    }
}

TEST_CASE("shell: Waveform peak decoding, interval aggregation, silence, impulses, speed, trim, zoom, and project reopen") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    // 1. Write silence WAV (48kHz, stereo, 2 seconds = 96000 samples per channel)
    QString silencePath = tempDir.filePath("silence.wav");
    std::vector<int16_t> silencePcm(96000 * 2, 0);
    writeWavFile(silencePath, 48000, 2, silencePcm);

    // 2. Write impulse WAV (48kHz, stereo, 2 seconds = 96000 samples per channel)
    // Put an impulse at 1.0s (samples 48000 to 48240, amplitude 30000 ~ 0.915)
    QString impulsePath = tempDir.filePath("impulse.wav");
    std::vector<int16_t> impulsePcm(96000 * 2, 0);
    for (int s = 48000 * 2; s < 48240 * 2; ++s) {
        impulsePcm[s] = 30000;
    }
    writeWavFile(impulsePath, 48000, 2, impulsePcm);

    shell::Session session;
    session.newProject();
    core::Sequence* seq = session.project().activeSequence();
    CHECK(seq != nullptr);

    // Asset 1: Silence
    core::Asset silenceAsset;
    silenceAsset.id = "asset-silence";
    silenceAsset.path = silencePath.toStdString();
    silenceAsset.kind = core::AssetKind::Audio;
    silenceAsset.duration = core::Rational(2, 1);
    session.project().assets[silenceAsset.id] = silenceAsset;

    // Asset 2: Impulse
    core::Asset impulseAsset;
    impulseAsset.id = "asset-impulse";
    impulseAsset.path = impulsePath.toStdString();
    impulseAsset.kind = core::AssetKind::Audio;
    impulseAsset.duration = core::Rational(2, 1);
    session.project().assets[impulseAsset.id] = impulseAsset;

    // Clip 1: Silence
    core::Clip cSilence;
    cSilence.id = "clip-silence";
    cSilence.assetId = "asset-silence";
    cSilence.sourceIn = core::Rational(0);
    cSilence.sourceOut = core::Rational(2);
    seq->clips[cSilence.id] = cSilence;

    // Clip 2: Impulse
    core::Clip cImpulse;
    cImpulse.id = "clip-impulse";
    cImpulse.assetId = "asset-impulse";
    cImpulse.sourceIn = core::Rational(0);
    cImpulse.sourceOut = core::Rational(2);
    seq->clips[cImpulse.id] = cImpulse;

    shell::TimelineModel timeline;
    timeline.setSession(&session);

    // Initial call initiates background decode
    timeline.clipAudioPeaks("clip-silence", 10);
    timeline.clipAudioPeaks("clip-impulse", 10);

    // Wait for background decoding to complete
    int initialVer = timeline.peaksVersion();
    QElapsedTimer timer;
    timer.start();
    while (timeline.peaksVersion() < initialVer + 2 && !timer.hasExpired(5000)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(20);
    }

    // A. Verify silence returns all 0.0
    QVariantList silencePeaks = timeline.clipAudioPeaks("clip-silence", 10);
    CHECK_EQ(static_cast<int>(silencePeaks.size()), 10);
    for (const auto& v : silencePeaks) {
        CHECK_EQ(v.toDouble(), 0.0);
    }

    // B. Verify known impulse is detected in bar covering 1.0s (bar 2 of 4 bars covering 2s)
    // 4 bars covering 2.0s -> each bar covers 0.5s:
    // Bar 0: 0.0 - 0.5s
    // Bar 1: 0.5 - 1.0s
    // Bar 2: 1.0 - 1.5s (Impulse is at 1.0s)
    // Bar 3: 1.5 - 2.0s
    QVariantList impulsePeaks = timeline.clipAudioPeaks("clip-impulse", 4);
    CHECK_EQ(static_cast<int>(impulsePeaks.size()), 4);
    CHECK(impulsePeaks[2].toDouble() >= 0.8); // Detected!
    CHECK(impulsePeaks[0].toDouble() < 0.1);
    CHECK(impulsePeaks[1].toDouble() < 0.1);
    CHECK(impulsePeaks[3].toDouble() < 0.1);

    // C. Verify trim shifts the impulse:
    // With sourceIn = 1.0s, the impulse (at media 1.0s) is now at the start of the clip!
    seq->clips["clip-impulse"].sourceIn = core::Rational(1, 1);
    seq->clips["clip-impulse"].sourceOut = core::Rational(2, 1);
    QVariantList trimmedPeaks = timeline.clipAudioPeaks("clip-impulse", 4);
    CHECK_EQ(static_cast<int>(trimmedPeaks.size()), 4);
    CHECK(trimmedPeaks[0].toDouble() >= 0.8); // Shifted to first bar!

    // D. Verify speed and zoom:
    seq->clips["clip-impulse"].sourceIn = core::Rational(0);
    seq->clips["clip-impulse"].speed = core::Rational(2, 1);
    QVariantList fastPeaks = timeline.clipAudioPeaks("clip-impulse", 10);
    CHECK_EQ(static_cast<int>(fastPeaks.size()), 10);

    // E. Verify reopening another project with reused asset ID isolates cache:
    shell::Session session2;
    session2.newProject();
    core::Sequence* seq2 = session2.project().activeSequence();
    core::Asset reusedAsset;
    reusedAsset.id = "asset-impulse"; // REUSED ID!
    reusedAsset.path = silencePath.toStdString(); // But pointing to SILENCE!
    reusedAsset.kind = core::AssetKind::Audio;
    reusedAsset.duration = core::Rational(2, 1);
    session2.project().assets[reusedAsset.id] = reusedAsset;

    core::Clip cReused;
    cReused.id = "clip-reused";
    cReused.assetId = "asset-impulse";
    cReused.sourceIn = core::Rational(0);
    cReused.sourceOut = core::Rational(2);
    seq2->clips[cReused.id] = cReused;

    timeline.setSession(&session2);
    timeline.clipAudioPeaks("clip-reused", 4);
    timer.restart();
    while (timeline.peaksVersion() < initialVer + 3 && !timer.hasExpired(5000)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(20);
    }
    QVariantList reusedPeaks = timeline.clipAudioPeaks("clip-reused", 4);
    // MUST BE SILENCE (0.0), NOT the old impulse from session 1!
    CHECK_EQ(reusedPeaks[2].toDouble(), 0.0);
}

TEST_CASE("shell: ThumbnailProvider safe asynchronous requests overlapping project mutations and media replacement") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    shell::Session session;
    session.newProject();

    // Create a real media asset using fixture
    core::Asset asset;
    asset.id = "asset-thumb";
    asset.path = FIXTURE_MP4;
    asset.duration = core::Rational(4, 1);
    session.project().assets[asset.id] = asset;

    shell::ThumbnailProvider provider(&session);

    std::atomic<bool> stopWorkers{false};
    std::atomic<int> successCount{0};
    std::vector<std::thread> workers;

    for (int t = 0; t < 3; ++t) {
        workers.emplace_back([&provider, &stopWorkers, &successCount]() {
            while (!stopWorkers.load()) {
                QSize sz;
                QImage img = provider.requestImage("asset-thumb?time=1.0", &sz, QSize(128, 128));
                if (!img.isNull()) {
                    successCount.fetch_add(1);
                }
                std::this_thread::yield();
            }
        });
    }

    // Simultaneously mutate project in main thread (newProject, replacement, open)
    for (int i = 0; i < 20; ++i) {
        session.newProject();
        core::Asset a2;
        a2.id = "asset-thumb";
        a2.path = FIXTURE_MP4;
        a2.duration = core::Rational(2, 1);
        session.project().assets[a2.id] = a2;
        provider.updateSnapshots();
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }

    stopWorkers.store(true);
    for (auto& w : workers) {
        w.join();
    }
    CHECK(successCount.load() > 0);
}

TEST_CASE("shell: TimelineModel clipInfo returns trackId and assetId") {
    ensureApp();
    shell::Session session;
    session.newProject();

    core::Sequence* seq = session.project().activeSequence();
    CHECK(seq != nullptr);
    CHECK(!seq->tracks.empty());

    core::Clip clip;
    clip.id = "clip-abc";
    clip.assetId = "asset-xyz";
    clip.name = "My Clip";
    clip.sourceIn = core::Rational(1);
    clip.sourceOut = core::Rational(5);
    clip.seqStart = core::Rational(2);

    std::string trackId = seq->tracks.front().id;
    seq->tracks.front().clipIds.push_back(clip.id);
    seq->clips.emplace(clip.id, clip);

    shell::TimelineModel timeline;
    timeline.setSession(&session);

    QVariantMap info = timeline.clipInfo("clip-abc");
    CHECK_EQ(info["clipId"].toString().toStdString(), "clip-abc");
    CHECK_EQ(info["trackId"].toString().toStdString(), trackId);
    CHECK_EQ(info["assetId"].toString().toStdString(), "asset-xyz");
    CHECK_EQ(info["name"].toString().toStdString(), "My Clip");
}

TEST_CASE("shell: TimelineModel color wheel gamma defaults to neutral") {
    ensureApp();
    shell::Session session;
    session.newProject();
    const QString clipId = session.addTitle("Gamma");
    CHECK(!clipId.isEmpty());

    shell::TimelineModel timeline;
    timeline.setSession(&session);
    CHECK_EQ(timeline.clipInfo(clipId)["gammaY"].toDouble(), 1.0);

    auto* seq = session.project().activeSequence();
    CHECK(seq != nullptr);
    auto* clip = seq->findClip(clipId.toStdString());
    CHECK(clip != nullptr);
    core::Effect wheels;
    wheels.type = "color_wheels";
    wheels.params["liftY"] = 0.1;
    clip->effects.push_back(wheels);
    CHECK_EQ(timeline.clipInfo(clipId)["gammaY"].toDouble(), 1.0);

    clip->effects.back().params["gammaY"] = 2.0;
    CHECK_EQ(timeline.clipInfo(clipId)["gammaY"].toDouble(), 2.0);
}

TEST_CASE("shell: ExportController resolution override and repeat export reset") {
    ensureApp();
    shell::Session session;
    session.newProject();

    shell::ExportController exporter;
    exporter.setSession(&session);

    CHECK_EQ(exporter.state(), shell::ExportController::Idle);
    exporter.setExportWidth(1920);
    exporter.setExportHeight(1080);
    CHECK_EQ(exporter.exportWidth(), 1920);
    CHECK_EQ(exporter.exportHeight(), 1080);
    CHECK(exporter.summary().contains("1920x1080"));

    // Reset works when not running
    exporter.reset();
    CHECK_EQ(exporter.state(), shell::ExportController::Idle);
}

TEST_CASE("shell: Recovery crash-safe immutable generations, atomic pointer, and fallback restoration") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    shell::Session session;
    session.setRecoveryDirectoryForTesting(tempDir.path());
    session.newProject();

    CHECK(!session.hasRecovery());

    // 1. Save generation 1
    session.addTitle("Title Snapshot 1");
    bool saved1 = session.saveRecovery();
    CHECK(saved1);
    CHECK(session.hasRecovery());

    QDir rDir(tempDir.path());
    CHECK(rDir.exists("gen_000001.json"));
    CHECK(rDir.exists("current.ptr"));

    // Check pointer content
    QFile ptrFile(rDir.filePath("current.ptr"));
    CHECK(ptrFile.open(QIODevice::ReadOnly));
    QString ptrContent = QString::fromUtf8(ptrFile.readAll()).trimmed();
    ptrFile.close();
    CHECK_EQ(ptrContent.toStdString(), "gen_000001.json");

    QVariantMap info1 = session.recoveryInfo();
    CHECK(info1["hasRecovery"].toBool());

    // 2. Save generation 2
    session.addTitle("Title Snapshot 2");
    bool saved2 = session.saveRecovery();
    CHECK(saved2);
    CHECK(rDir.exists("gen_000002.json"));

    CHECK(ptrFile.open(QIODevice::ReadOnly));
    ptrContent = QString::fromUtf8(ptrFile.readAll()).trimmed();
    ptrFile.close();
    CHECK_EQ(ptrContent.toStdString(), "gen_000002.json");

    // 3. Crash resilience: Corrupt current.ptr and verify fallback to highest valid generation
    CHECK(ptrFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ptrFile.write("corrupt_nonexistent_ptr_data");
    ptrFile.close();

    // Re-instantiate session in same recovery dir to simulate restart after crash
    shell::Session sessionRecover;
    sessionRecover.setRecoveryDirectoryForTesting(tempDir.path());
    CHECK(sessionRecover.hasRecovery()); // Must succeed via fallback to gen_000002.json!

    bool restored = sessionRecover.restoreRecovery();
    CHECK(restored);
    const core::Sequence* recSeq = sessionRecover.project().activeSequence();
    CHECK(recSeq != nullptr);
    // Should contain both titles
    CHECK_EQ(static_cast<int>(recSeq->clips.size()), 2);

    // 4. Autosave failure signaling: set unwritable path
    QString unwritablePath = "Z:/nonexistent_drive_12345/unwritable/recovery";
    shell::Session sessionFail;
    sessionFail.setRecoveryDirectoryForTesting(unwritablePath);
    sessionFail.newProject();
    sessionFail.addTitle("Title Unsaved");

    bool signalReceived = false;
    QObject::connect(&sessionFail, &shell::Session::autosaveFailed, [&signalReceived](const QString&) {
        signalReceived = true;
    });
    bool saveOk = sessionFail.saveRecovery();
    CHECK(!saveOk);
    CHECK(signalReceived);

    // 5. Clear recovery
    shell::Session::setRecoveryDirectoryForTesting(tempDir.path());
    sessionRecover.clearRecovery();
    CHECK(!sessionRecover.hasRecovery());
    rDir.refresh();
    CHECK(!rDir.exists("gen_000002.json"));
    CHECK(!rDir.exists("current.ptr"));
}

TEST_CASE("shell: Fractional recovery hashes round-trip and reject tampered generations") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());
    shell::Session session;
    session.setRecoveryDirectoryForTesting(tempDir.path());
    const QString clipId = session.addTitle("Fractional recovery");
    CHECK(!clipId.isEmpty());
    CHECK(session.setClipOpacity(clipId, 0.1));
    CHECK(session.setClipTransform(clipId, 1.1, 0.2, -0.3, 12.7));
    CHECK(session.moveClipTo(clipId, 0.125));
    CHECK(session.saveRecovery());
    CHECK(session.hasRecovery());
    CHECK(session.recoveryInfo()["hasRecovery"].toBool());

    QFile generation(tempDir.filePath("gen_000001.json"));
    CHECK(generation.open(QIODevice::ReadOnly));
    const QJsonDocument envelope = QJsonDocument::fromJson(generation.readAll());
    generation.close();
    CHECK(envelope.isObject());
    const QByteArray payload = QJsonDocument(envelope.object().value("project").toObject())
                                  .toJson(QJsonDocument::Compact);
    CHECK_EQ(envelope.object().value("contentHash").toString().toUtf8(),
             QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());

    shell::Session restored;
    CHECK(restored.restoreRecovery());
    const core::Sequence* seq = restored.project().activeSequence();
    CHECK(seq != nullptr);
    const core::Clip* clip = seq->findClip(clipId.toStdString());
    CHECK(clip != nullptr);
    CHECK_EQ(clip->opacity, 0.1);
    CHECK_EQ(clip->transform.scale, 1.1);
    CHECK_EQ(clip->transform.x, 0.2);
    CHECK_EQ(clip->transform.y, -0.3);
    CHECK_EQ(clip->transform.rotationDeg, 12.7);
    CHECK_EQ(clip->seqStart, core::Rational(1, 8));
    CHECK(restored.dirty());
    CHECK(restored.filePath().isEmpty());

    CHECK(session.setClipOpacity(clipId, 0.7));
    CHECK(session.saveRecovery());
    CHECK(QFile::remove(tempDir.filePath("current.ptr")));
    CHECK(restored.restoreRecovery());
    CHECK_EQ(restored.project().activeSequence()->findClip(clipId.toStdString())->opacity, 0.7);

    QFile newest(tempDir.filePath("gen_000002.json"));
    CHECK(newest.open(QIODevice::ReadOnly));
    QJsonObject altered = QJsonDocument::fromJson(newest.readAll()).object();
    newest.close();
    QJsonObject alteredProject = altered.value("project").toObject();
    alteredProject["name"] = "Tampered";
    altered["project"] = alteredProject;
    const QByteArray alteredBytes = QJsonDocument(altered).toJson(QJsonDocument::Compact);
    CHECK(newest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    CHECK_EQ(newest.write(alteredBytes), alteredBytes.size());
    newest.close();
    CHECK(restored.restoreRecovery());
    CHECK_EQ(restored.project().activeSequence()->findClip(clipId.toStdString())->opacity, 0.1);
    CHECK(QFile::remove(tempDir.filePath("gen_000001.json")));
    CHECK(!restored.hasRecovery());
    CHECK(!restored.restoreRecovery());
}

TEST_CASE("shell: Failed generation and pointer writes preserve prior fractional recovery") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());
    shell::Session session;
    session.setRecoveryDirectoryForTesting(tempDir.path());
    const QString clipId = session.addTitle("Prior recovery");
    CHECK(!clipId.isEmpty());
    CHECK(session.setClipOpacity(clipId, 0.1));
    CHECK(session.saveRecovery());

    QFile prior(tempDir.filePath("gen_000001.json"));
    CHECK(prior.open(QIODevice::ReadOnly));
    const QByteArray priorBytes = prior.readAll();
    prior.close();
    QFile pointer(tempDir.filePath("current.ptr"));
    CHECK(pointer.open(QIODevice::ReadOnly));
    const QByteArray pointerBytes = pointer.readAll();
    pointer.close();

    int failures = 0;
    QObject::connect(&session, &shell::Session::autosaveFailed, [&failures](const QString&) {
        ++failures;
    });
    CHECK(session.setClipOpacity(clipId, 0.7));
    QDir dir(tempDir.path());
    CHECK(dir.mkdir("gen_000002.json"));
    CHECK(!session.saveRecovery());
    CHECK_EQ(failures, 1);
    CHECK(pointer.open(QIODevice::ReadOnly));
    CHECK_EQ(pointer.readAll(), pointerBytes);
    pointer.close();
    shell::Session restored;
    CHECK(restored.restoreRecovery());
    CHECK_EQ(restored.project().activeSequence()->findClip(clipId.toStdString())->opacity, 0.1);
    CHECK(dir.rmdir("gen_000002.json"));

    CHECK(pointer.rename(tempDir.filePath("saved.ptr")));
    CHECK(dir.mkdir("current.ptr"));
    CHECK(!session.saveRecovery());
    CHECK_EQ(failures, 2);
    CHECK(QFileInfo(tempDir.filePath("current.ptr")).isDir());
    CHECK(prior.open(QIODevice::ReadOnly));
    CHECK_EQ(prior.readAll(), priorBytes);
    prior.close();
    CHECK(dir.rmdir("current.ptr"));
    CHECK(pointer.rename(tempDir.filePath("current.ptr")));
    CHECK(restored.restoreRecovery());
    CHECK_EQ(restored.project().activeSequence()->findClip(clipId.toStdString())->opacity, 0.1);
    CHECK(session.dirty());
    CHECK(session.filePath().isEmpty());
}

TEST_CASE("shell: Audio import, timeline placement, track mute, and Player volume/mute controls") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    // Write a test 440 Hz WAV
    QString wavPath = tempDir.filePath("sine440.wav");
    std::vector<int16_t> pcm(48000 * 2, 0); // 1 second of stereo tone
    for (size_t i = 0; i < 48000; ++i) {
        int16_t val = static_cast<int16_t>(16384.0 * std::sin(2.0 * 3.141592653589793 * 440.0 * i / 48000.0));
        pcm[i * 2 + 0] = val;
        pcm[i * 2 + 1] = val;
    }
    writeWavFile(wavPath, 48000, 2, pcm);

    shell::Session session;
    session.setRecoveryDirectoryForTesting(tempDir.path());
    session.newProject();

    // 1. Import audio media
    QString assetId = session.importMedia(QUrl::fromLocalFile(wavPath));
    CHECK(!assetId.isEmpty());

    const auto& assets = session.project().assets;
    auto ait = assets.find(assetId.toStdString());
    CHECK(ait != assets.end());
    CHECK(ait->second.hasAudio);
    CHECK_EQ(static_cast<int>(ait->second.kind), static_cast<int>(core::AssetKind::Audio));

    // 2. Add clip to timeline -> places on A1
    QString clipId = session.addClipToTimeline(assetId);
    CHECK(!clipId.isEmpty());

    const core::Sequence* seq = session.project().activeSequence();
    CHECK(seq != nullptr);
    bool foundOnA1 = false;
    for (const auto& track : seq->tracks) {
        if (track.kind == core::TrackKind::Audio) {
            for (const auto& cid : track.clipIds) {
                if (cid == clipId.toStdString()) {
                    foundOnA1 = true;
                    break;
                }
            }
        }
    }
    CHECK(foundOnA1);

    // 3. Track mute testing
    session.setTrackMuted("A1", true);
    seq = session.project().activeSequence();
    bool a1Muted = false;
    for (const auto& track : seq->tracks) {
        if (track.name == "A1" || track.id == "A1") {
            a1Muted = track.muted;
        }
    }
    CHECK(a1Muted);

    // 4. TimelineModel exposes muted, locked, visible
    shell::TimelineModel timeline;
    timeline.setSession(&session);
    QVariantList tracks = timeline.tracks();
    bool modelReportedMuted = false;
    for (const auto& t : tracks) {
        QVariantMap tm = t.toMap();
        if (tm["name"].toString() == "A1" || tm["trackId"].toString() == "A1") {
            modelReportedMuted = tm["muted"].toBool();
        }
    }
    CHECK(modelReportedMuted);

    session.setTrackMuted("A1", false);
    tracks = timeline.tracks();
    for (const auto& t : tracks) {
        QVariantMap tm = t.toMap();
        if (tm["name"].toString() == "A1" || tm["trackId"].toString() == "A1") {
            CHECK(!tm["muted"].toBool());
        }
    }

    // 5. Player properties, volume, and mute controls
    shell::Player player;
    player.setSession(&session);
    CHECK_EQ(player.volume(), 1.0);
    CHECK_EQ(player.muted(), false);

    player.setVolume(0.65);
    CHECK_EQ(player.volume(), 0.65);

    player.setMuted(true);
    CHECK_EQ(player.muted(), true);

    player.setMuted(false);
    CHECK_EQ(player.muted(), false);
}

TEST_CASE("shell: extractAudioFromClip decouples audio onto timeline and extractAudioToFile exports valid WAV") {
    ensureApp();
    QTemporaryDir tempDir;
    CHECK(tempDir.isValid());

    shell::Session session;
    session.setRecoveryDirectoryForTesting(tempDir.path());
    session.newProject();

    // 1. Import fixture video media (contains 8s 1280x720 video and AAC audio)
    QString assetId = session.importMedia(QUrl::fromLocalFile(FIXTURE_MP4));
    CHECK(!assetId.isEmpty());

    // 2. Add video clip to timeline
    QString videoClipId = session.addClipToTimeline(assetId);
    CHECK(!videoClipId.isEmpty());
    CHECK(session.clipHasAudio(videoClipId));

    // 3. Extract audio from video clip onto timeline
    bool extracted = session.extractAudioFromClip(videoClipId);
    CHECK(extracted);

    // Verify audio track contains extracted audio clip
    const core::Sequence* seq = session.project().activeSequence();
    CHECK(seq != nullptr);
    bool foundExtracted = false;
    for (const auto& track : seq->tracks) {
        if (track.kind == core::TrackKind::Audio) {
            for (const auto& cid : track.clipIds) {
                if (const core::Clip* c = seq->findClip(cid)) {
                    if (c->name.find("(Audio)") != std::string::npos && c->assetId == assetId.toStdString()) {
                        foundExtracted = true;
                        CHECK_EQ(c->sourceIn, core::Rational(0));
                        CHECK_EQ(c->seqStart, core::Rational(0));
                        break;
                    }
                }
            }
        }
    }
    CHECK(foundExtracted);

    // 4. Second extraction at same time -> collision detection creates A2
    bool secondExtracted = session.extractAudioFromClip(videoClipId);
    CHECK(secondExtracted);
    seq = session.project().activeSequence();
    int audioTrackCount = 0;
    for (const auto& track : seq->tracks) {
        if (track.kind == core::TrackKind::Audio) {
            audioTrackCount++;
        }
    }
    CHECK(audioTrackCount >= 2);

    // 5. Extract audio to standalone WAV file
    QString destWav = tempDir.filePath("extracted_test.wav");
    QString resPath = session.extractAudioToFile(videoClipId, destWav);
    CHECK(!resPath.isEmpty());
    CHECK(QFile::exists(destWav));
    QFileInfo fi(destWav);
    CHECK(fi.size() > 44); // Header is 44 bytes, plus decoded PCM

    // Verify WAV RIFF and WAVE header
    QFile rf(destWav);
    CHECK(rf.open(QIODevice::ReadOnly));
    QByteArray header = rf.read(12);
    rf.close();
    CHECK_EQ(header.left(4), QByteArray("RIFF"));
    CHECK_EQ(header.mid(8, 4), QByteArray("WAVE"));

    // 6. Player seekTo while paused resets pump and updates position accurately
    shell::Player player;
    player.setSession(&session);
    player.seekTo(3.5);
    CHECK_EQ(player.positionSec(), 3.5);
    CHECK(!player.playing());

    // Seeking to 0
    player.seekTo(0.0);
    CHECK_EQ(player.positionSec(), 0.0);

    // Seeking forward to 6.0
    player.seekTo(6.0);
    CHECK_EQ(player.positionSec(), 6.0);
}

int main() {
    return editor::tests::runAll();
}
