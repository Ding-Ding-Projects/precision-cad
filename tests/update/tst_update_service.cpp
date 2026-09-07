#include <QtTest>

#include "update/update_service.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

using namespace precision::update;

namespace {
struct ScriptedResponse {
  int status = 200;
  QUrl finalUrl;
  QList<QByteArray> chunks;
  QString error;
};

class FakeTransport final : public UpdateTransport {
public:
  QHash<QString, ScriptedResponse> responses;
  QList<TransferRequest> requests;
  bool cancelled = false;
  bool defer = false;
  TransferRequest pendingRequest;
  TransferSink pendingSink;
  std::function<void(TransferResult)> pendingCompletion;

  void get(const TransferRequest &request, TransferSink sink,
           std::function<void(TransferResult)> completed) override {
    requests.append(request);
    if (defer) {
      pendingRequest = request;
      pendingSink = std::move(sink);
      pendingCompletion = std::move(completed);
      return;
    }
    complete(request, std::move(sink), std::move(completed));
  }

  void deliverPending() {
    QVERIFY(pendingCompletion);
    complete(pendingRequest, std::move(pendingSink), std::move(pendingCompletion));
  }

  void cancel() override { cancelled = true; }

private:
  void complete(const TransferRequest &request, TransferSink sink,
                std::function<void(TransferResult)> completed) {
    const auto response = responses.value(request.url.toString());
    TransferResult result;
    result.status = response.status;
    result.finalUrl = response.finalUrl.isValid() ? response.finalUrl : request.url;
    result.error = response.error;
    for (const auto &chunk : response.chunks) {
      if (!result.error.isEmpty()) break;
      result.bytes += chunk.size();
      if (!sink(chunk)) result.error = QStringLiteral("sink refused streamed data");
    }
    completed(std::move(result));
  }
};

class FakeProcess final : public UpdateProcess {
public:
  bool called = false;
  QString program;
  QStringList arguments;
  std::function<void()> onStarted;
  std::function<void(QString)> onCompleted;

  void start(const QString &requestedProgram, const QStringList &requestedArguments,
             std::function<void()> started, std::function<void(QString)> completed) override {
    called = true;
    program = requestedProgram;
    arguments = requestedArguments;
    onStarted = std::move(started);
    onCompleted = std::move(completed);
  }

  void emitStarted() { QVERIFY(onStarted); onStarted(); }
  void emitCompleted(QString error = {}) { QVERIFY(onCompleted); onCompleted(std::move(error)); }
};

QByteArray sha1(const QByteArray &bytes) {
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex();
}

QByteArray sha256(const QByteArray &bytes) {
  return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

class TrustedNetworkAccessManager final : public QNetworkAccessManager {
public:
  explicit TrustedNetworkAccessManager(const QSslCertificate &certificate) : m_certificate(certificate) {}
protected:
  QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *outgoingData) override {
    QNetworkRequest trusted(request);
    auto configuration = trusted.sslConfiguration();
    auto authorities = configuration.caCertificates();
    authorities.append(m_certificate);
    configuration.setCaCertificates(authorities);
    configuration.setPeerVerifyMode(QSslSocket::VerifyPeer);
    trusted.setSslConfiguration(configuration);
    return QNetworkAccessManager::createRequest(operation, trusted, outgoingData);
  }
private:
  QSslCertificate m_certificate;
};

class LocalTlsServer final : public QTcpServer {
public:
  explicit LocalTlsServer(const QSslCertificate &certificate, const QSslKey &key) : m_certificate(certificate), m_key(key) {}
  QByteArray response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}";
  bool closeResponse = true;
  int requests = 0, disconnected = 0;
protected:
  void incomingConnection(qintptr descriptor) override {
    auto *socket = new QSslSocket(this);
    socket->setLocalCertificate(m_certificate);
    socket->setPrivateKey(m_key);
    if (!socket->setSocketDescriptor(descriptor)) { socket->deleteLater(); return; }
    connect(socket, &QSslSocket::disconnected, this, [this, socket] { ++disconnected; socket->deleteLater(); });
    connect(socket, &QSslSocket::readyRead, socket, [this, socket, request = QByteArray{}]( ) mutable {
      request.append(socket->readAll());
      if (!request.contains("\r\n\r\n") || socket->property("answered").toBool()) return;
      socket->setProperty("answered", true);
      ++requests;
      socket->write(response);
      if (closeResponse) socket->disconnectFromHost();
    });
    socket->startServerEncryption();
  }
private:
  QSslCertificate m_certificate;
  QSslKey m_key;
};
}

class UpdateServiceTest final : public QObject {
  Q_OBJECT

  static UpdateConfig config(const QTemporaryDir &dir) {
    return {QStringLiteral("0.1.0"),
            QUrl(QStringLiteral("https://updates.example.test/release/metadata.json")),
            dir.filePath("staging"), 1};
  }

  static QString executablePath(const QTemporaryDir &dir) {
    return dir.filePath("app-0.1.0/PrecisionCAD.exe");
  }

  static void writeFile(const QString &path, const QByteArray &content) {
    QVERIFY(QDir().mkpath(QFileInfo(path).dir().absolutePath()));
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
    QCOMPARE(file.write(content), content.size());
  }

  static void installShape(const QTemporaryDir &dir, const QByteArray &localPackage = "installed-package") {
    writeFile(executablePath(dir), "fake executable");
    writeFile(dir.filePath("Update.exe"), "fake updater");
    const QString packageName = QStringLiteral("PrecisionCAD-0.1.0-full.nupkg");
    writeFile(dir.filePath(QStringLiteral("packages/") + packageName), localPackage);
    writeFile(dir.filePath("packages/RELEASES"), sha1(localPackage) + " " + packageName.toUtf8() + " " + QByteArray::number(localPackage.size()) + "\n");
  }

  static QByteArray metadata(const QByteArray &package, const QString &version = QStringLiteral("0.1.1")) {
    return QJsonDocument(QJsonObject{
      {"version", version},
      {"notesUrl", "https://updates.example.test/release/notes/0.1.1"},
      {"packageUrl", "https://updates.example.test/release/PrecisionCAD-0.1.1-full.nupkg"},
      {"sha256", QString::fromLatin1(sha256(package))},
    }).toJson(QJsonDocument::Compact);
  }

  static void seedCandidate(FakeTransport *transport, const UpdateConfig &cfg,
                            const QByteArray &package, const QByteArray &releases = {}) {
    const QUrl manifest = cfg.feedUrl.resolved(QUrl(QStringLiteral("RELEASES")));
    const QUrl packageUrl(QStringLiteral("https://updates.example.test/release/PrecisionCAD-0.1.1-full.nupkg"));
    transport->responses.insert(cfg.feedUrl.toString(), {200, cfg.feedUrl, {metadata(package)}, {}});
    transport->responses.insert(manifest.toString(), {200, manifest,
      {releases.isEmpty() ? sha1(package) + " PrecisionCAD-0.1.1-full.nupkg " + QByteArray::number(package.size()) + "\n" : releases}, {}});
    transport->responses.insert(packageUrl.toString(), {200, packageUrl, {package}, {}});
  }

  static QString singleStageDir(const UpdateConfig &cfg) {
    const auto children = QDir(cfg.stagingDirectory).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    return children.size() == 1 ? QDir(cfg.stagingDirectory).filePath(children.constFirst()) : QString{};
  }

private slots:
  void rejectsUntrustedAndDowngradeMetadata() {
    QTemporaryDir dir;
    UpdateInfo info;
    QString error;
    const auto cfg = config(dir);
    QVERIFY(!UpdateService::parseMetadata(metadata("x", QStringLiteral("0.1.0")), cfg, &info, &error));
    const QByteArray unsafe = R"({"version":"0.1.1","notesUrl":"https://updates.example.test/a","packageUrl":"https://evil.test/PrecisionCAD-0.1.1-full.nupkg","sha256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"})";
    QVERIFY(!UpdateService::parseMetadata(unsafe, cfg, &info, &error));
  }

  void rejectsTraversalAndBadManifestRows() {
    QVERIFY(!UpdateService::isSafePackageName(QStringLiteral("../PrecisionCAD-full.nupkg")));
    QVERIFY(!UpdateService::isSafePackageName(QStringLiteral("PrecisionCAD-delta.nupkg")));
    UpdateInfo info;
    info.packageUrl = QUrl(QStringLiteral("https://updates.example.test/release/PrecisionCAD-0.1.1-full.nupkg"));
    QString error;
    QVERIFY(!UpdateService::verifyReleases("0123 ../PrecisionCAD-0.1.1-full.nupkg 7", info, &error));
    QVERIFY(!UpdateService::verifyReleases("not-a-sha PrecisionCAD-0.1.1-full.nupkg 7", info, &error));
    QVERIFY(!UpdateService::verifyReleases("0123456789012345678901234567890123456789 PrecisionCAD-0.1.1-full.nupkg 0", info, &error));
    const QByteArray row("0123456789012345678901234567890123456789 PrecisionCAD-0.1.1-full.nupkg 7\n");
    QVERIFY(!UpdateService::verifyReleases(row + row, info, &error));
  }

  void installedDiscoveryRequiresCompleteSquirrelShape() {
    QTemporaryDir dir;
    auto cfg = config(dir);
    UpdateService service(cfg);
    service.setExecutablePathForTesting(executablePath(dir));
    QVERIFY(!service.isInstalledSquirrelApplication());
    installShape(dir);
    QVERIFY(service.isInstalledSquirrelApplication());
    QVERIFY(QFile::remove(dir.filePath("packages/RELEASES")));
    QVERIFY(!service.isInstalledSquirrelApplication());
  }

  void streamsCandidateToUniqueStageAndRequiresApproval() {
    QTemporaryDir dir;
    installShape(dir);
    auto cfg = config(dir);
    const QByteArray package("candidate-package");
    auto transport = std::make_unique<FakeTransport>();
    auto *rawTransport = transport.get();
    seedCandidate(rawTransport, cfg, package);
    auto process = std::make_unique<FakeProcess>();
    auto *rawProcess = process.get();

    UpdateService service(cfg);
    service.setExecutablePathForTesting(executablePath(dir));
    service.setTransport(std::move(transport));
    service.setProcess(std::move(process));
    QSignalSpy approvals(&service, &UpdateService::restartApprovalRequested);

    service.checkNow();
    QCOMPARE(service.state(), UpdateState::Ready);
    QCOMPARE(rawTransport->requests.size(), 3);
    const QString stage = singleStageDir(cfg);
    QVERIFY(!stage.isEmpty());
    QCOMPARE(QFileInfo(stage + "/PrecisionCAD-0.1.1-full.nupkg").size(), package.size());
    QFile stagedManifest(stage + "/RELEASES");
    QVERIFY(stagedManifest.open(QIODevice::ReadOnly));
    QCOMPARE(stagedManifest.readAll(), sha1(package) + " PrecisionCAD-0.1.1-full.nupkg " + QByteArray::number(package.size()) + "\n");
    stagedManifest.close();

    service.requestInstall();
    QCOMPARE(service.state(), UpdateState::AwaitingApproval);
    QCOMPARE(approvals.count(), 1);
    const auto approval = approvals.takeFirst();
    QCOMPARE(approval.at(1).toString(), QStringLiteral("0.1.1"));
    const QString approvalId = approval.at(0).toString();
    QVERIFY(!approvalId.isEmpty());
    QVERIFY(!rawProcess->called);

    service.approveInstall(QStringLiteral("stale-approval"), true);
    QCOMPARE(service.state(), UpdateState::AwaitingApproval);
    QVERIFY(!rawProcess->called);
    service.approveInstall(approvalId, false);
    QCOMPARE(service.state(), UpdateState::Ready);
    service.requestInstall();
    const QString currentApproval = approvals.takeFirst().at(0).toString();
    service.approveInstall(currentApproval, true);
    QCOMPARE(service.state(), UpdateState::Starting);
    QVERIFY(rawProcess->called);
    QCOMPARE(rawProcess->program, QDir::cleanPath(dir.filePath("Update.exe")));
    QVERIFY(rawProcess->arguments.constFirst().startsWith(QStringLiteral("--update=")));
    QVERIFY(!rawProcess->arguments.constFirst().contains(QStringLiteral("file:///")));
    rawProcess->emitStarted();
    QCOMPARE(service.state(), UpdateState::Installing);
    rawProcess->emitCompleted();
    QCOMPARE(service.state(), UpdateState::Installed);
  }

  void manifestOrPackageTamperingRefusesReady() {
    QTemporaryDir dir;
    installShape(dir);
    auto cfg = config(dir);
    const QByteArray package("candidate-package");
    auto transport = std::make_unique<FakeTransport>();
    auto *raw = transport.get();
    seedCandidate(raw, cfg, package, sha1(package) + " PrecisionCAD-0.1.1-full.nupkg " + QByteArray::number(package.size() + 1) + "\n");
    UpdateService service(cfg);
    service.setExecutablePathForTesting(executablePath(dir));
    service.setTransport(std::move(transport));
    service.checkNow();
    QCOMPARE(service.state(), UpdateState::Error);

    UpdateService hashService(cfg);
    hashService.setExecutablePathForTesting(executablePath(dir));
    auto goodTransport = std::make_unique<FakeTransport>();
    auto *rawGood = goodTransport.get();
    seedCandidate(rawGood, cfg, package);
    rawGood->responses[QStringLiteral("https://updates.example.test/release/PrecisionCAD-0.1.1-full.nupkg")].chunks = {"tampered-package!"};
    hashService.setTransport(std::move(goodTransport));
    hashService.checkNow();
    QCOMPARE(hashService.state(), UpdateState::Error);
  }

  void cancellationInvalidatesSlowTransportCompletion() {
    QTemporaryDir dir;
    installShape(dir);
    auto cfg = config(dir);
    auto transport = std::make_unique<FakeTransport>();
    auto *raw = transport.get();
    raw->defer = true;
    raw->responses.insert(cfg.feedUrl.toString(), {200, cfg.feedUrl, {metadata("candidate-package")}, {}});
    UpdateService service(cfg);
    service.setExecutablePathForTesting(executablePath(dir));
    service.setTransport(std::move(transport));
    service.checkNow();
    QCOMPARE(service.state(), UpdateState::Checking);
    service.cancel();
    QVERIFY(raw->cancelled);
    QCOMPARE(service.state(), UpdateState::Idle);
    raw->deliverPending();
    QCOMPARE(service.state(), UpdateState::Idle);
  }

  void standaloneBuildIsHonestUnavailable() {
    QTemporaryDir dir;
    auto cfg = config(dir);
    UpdateService service(cfg);
    service.setExecutablePathForTesting(executablePath(dir));
    service.startupCheck();
    QCOMPARE(service.state(), UpdateState::Unavailable);
  }

  void refusesStagedMutationAtApproval_data() {
    QTest::addColumn<QString>("target");
    QTest::newRow("package") << QStringLiteral("PrecisionCAD-0.1.1-full.nupkg");
    QTest::newRow("manifest") << QStringLiteral("RELEASES");
  }

  void refusesStagedMutationAtApproval() {
    QFETCH(QString, target);
    QTemporaryDir dir;
    installShape(dir);
    auto cfg = config(dir);
    auto transport = std::make_unique<FakeTransport>();
    seedCandidate(transport.get(), cfg, "candidate-package");
    auto process = std::make_unique<FakeProcess>();
    auto *rawProcess = process.get();
    UpdateService service(cfg);
    service.setExecutablePathForTesting(executablePath(dir));
    service.setTransport(std::move(transport));
    service.setProcess(std::move(process));
    QSignalSpy approvals(&service, &UpdateService::restartApprovalRequested);
    service.checkNow();
    QCOMPARE(service.state(), UpdateState::Ready);
    service.requestInstall();
    const QString id = approvals.takeFirst().at(0).toString();
    const QString stage = singleStageDir(cfg);
    QVERIFY(!stage.isEmpty());
    QFile file(QDir(stage).filePath(target));
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.seek(0));
    QCOMPARE(file.write("!"), 1);
    file.close();
    service.approveInstall(id, true);
    QCOMPARE(service.state(), UpdateState::Error);
    QVERIFY(!rawProcess->called);
    QVERIFY(!QFileInfo::exists(stage));
  }

  void priorApprovalCannotAuthorizeNewGeneration() {
    QTemporaryDir dir;
    installShape(dir);
    auto cfg = config(dir);
    auto transport = std::make_unique<FakeTransport>();
    seedCandidate(transport.get(), cfg, "candidate-package");
    auto process = std::make_unique<FakeProcess>();
    auto *rawProcess = process.get();
    UpdateService service(cfg);
    service.setExecutablePathForTesting(executablePath(dir));
    service.setTransport(std::move(transport));
    service.setProcess(std::move(process));
    QSignalSpy approvals(&service, &UpdateService::restartApprovalRequested);
    service.checkNow(); service.requestInstall();
    const QString oldId = approvals.takeFirst().at(0).toString();
    const QString oldStage = singleStageDir(cfg);
    service.cancel();
    QVERIFY(!QFileInfo::exists(oldStage));
    service.checkNow(); service.requestInstall();
    const QString newId = approvals.takeFirst().at(0).toString();
    QVERIFY(oldId != newId);
    QVERIFY(oldStage != singleStageDir(cfg));
    service.approveInstall(oldId, true);
    QCOMPARE(service.state(), UpdateState::AwaitingApproval);
    QVERIFY(!rawProcess->called);
    service.approveInstall(newId, true);
    QVERIFY(rawProcess->called);
    rawProcess->emitStarted();
    rawProcess->emitCompleted(QStringLiteral("fixture child failed"));
    QCOMPARE(service.state(), UpdateState::Error);
  }

  void realProcessReportsExitAndStartFailures() {
    QTemporaryDir dir;
    for (const int exitCode : {0, 37}) {
      auto process = makeQtUpdateProcess();
      int starts = 0, completions = 0;
      QString error;
      process->start(QCoreApplication::applicationFilePath(),
        {QStringLiteral("--update-child"), dir.filePath("marker"), QString::number(exitCode)},
        [&] { ++starts; }, [&](QString result) { error = result; ++completions; });
      QTRY_COMPARE_WITH_TIMEOUT(starts, 1, 3000);
      QCOMPARE(completions, 0);
      QTRY_COMPARE_WITH_TIMEOUT(completions, 1, 3000);
      QCOMPARE(error.isEmpty(), exitCode == 0);
      if (exitCode) QVERIFY(error.contains(QString::number(exitCode)));
    }
    auto process = makeQtUpdateProcess();
    int starts = 0, completions = 0;
    QString error;
    process->start(dir.filePath("missing-updater.exe"), {}, [&] { ++starts; }, [&](QString result) { error = result; ++completions; });
    QTRY_COMPARE_WITH_TIMEOUT(completions, 1, 3000);
    QCOMPARE(starts, 0);
    QVERIFY(!error.isEmpty());
  }

  void realProcessSurvivesAdapterDestruction() {
    QTemporaryDir dir;
    const QString marker = dir.filePath("child-completed.txt");
    auto process = makeQtUpdateProcess();
    bool started = false;
    process->start(QCoreApplication::applicationFilePath(), {QStringLiteral("--update-child"), marker},
                   [&started] { started = true; }, [](QString) {});
    QTRY_VERIFY_WITH_TIMEOUT(started, 3000);
    process.reset();
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(marker), 3000);
  }

  void realQtTransportRejectsUntrustedTlsThenAcceptsTrustedTls() {
    QTemporaryDir dir;
    const QString keyPath = dir.filePath("key.pem");
    const QString certificatePath = dir.filePath("certificate.pem");
    const QString configPath = dir.filePath("openssl.cnf");
    writeFile(configPath, "openssl_conf = openssl_init\n[openssl_init]\nproviders = provider_sect\n[provider_sect]\ndefault = default_sect\n[default_sect]\nactivate = 1\n");
    const QString opensslPath = QStandardPaths::findExecutable(QStringLiteral("openssl"));
    QVERIFY2(!opensslPath.isEmpty(), "OpenSSL is required to create this test-only localhost certificate.");
    QProcess openssl;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("OPENSSL_CONF"), configPath);
    openssl.setProcessEnvironment(environment);
    openssl.start(opensslPath, {
      QStringLiteral("req"), QStringLiteral("-x509"), QStringLiteral("-newkey"), QStringLiteral("rsa:2048"),
      QStringLiteral("-nodes"), QStringLiteral("-keyout"), keyPath, QStringLiteral("-out"), certificatePath,
      QStringLiteral("-days"), QStringLiteral("1"), QStringLiteral("-subj"), QStringLiteral("/CN=localhost"),
      QStringLiteral("-addext"), QStringLiteral("subjectAltName=DNS:localhost")});
    QVERIFY(openssl.waitForFinished(15000));
    QVERIFY2(openssl.exitCode() == 0, openssl.readAllStandardError().constData());
    QFile certificateFile(certificatePath);
    QVERIFY(certificateFile.open(QIODevice::ReadOnly));
    const QSslCertificate certificate(certificateFile.readAll(), QSsl::Pem);
    QVERIFY(!certificate.isNull());
    QFile keyFile(keyPath);
    QVERIFY(keyFile.open(QIODevice::ReadOnly));
    const QSslKey key(keyFile.readAll(), QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    QVERIFY(!key.isNull());
    LocalTlsServer server(certificate, key);
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QUrl url(QStringLiteral("https://localhost:%1/update").arg(server.serverPort()));

    bool untrustedDone = false;
    TransferResult untrusted;
    auto defaultTransport = makeQtUpdateTransport();
    defaultTransport->get({url, 1024, 2, 5000}, [](const QByteArray &) { return true; },
                          [&untrustedDone, &untrusted](TransferResult result) { untrusted = std::move(result); untrustedDone = true; });
    QTRY_VERIFY_WITH_TIMEOUT(untrustedDone, 7000);
    QVERIFY(!untrusted.error.isEmpty());

    TrustedNetworkAccessManager trustedManager(certificate);
    auto trustedTransport = makeQtUpdateTransport(&trustedManager);
    QByteArray body;
    bool trustedDone = false;
    TransferResult trusted;
    trustedTransport->get({url, 1024, 2, 5000}, [&body](const QByteArray &chunk) { body.append(chunk); return true; },
                          [&trustedDone, &trusted](TransferResult result) { trusted = std::move(result); trustedDone = true; });
    QTRY_VERIFY_WITH_TIMEOUT(trustedDone, 7000);
    QCOMPARE(trusted.error, QString{});
    QCOMPARE(trusted.status, 200);
    QCOMPARE(trusted.bytes, 2);
    QCOMPARE(body, QByteArray("{}"));

    const auto runResponse = [&](const QByteArray &response, qint64 cap, qint64 expected,
                                 bool close, bool cancelInSink, bool success, int timeout) {
      server.response = response;
      server.closeResponse = close;
      const int requestBaseline = server.requests;
      const int disconnectBaseline = server.disconnected;
      bool done = false;
      int completions = 0;
      qint64 delivered = 0, largestChunk = 0;
      TransferResult result;
      trustedTransport->get({url, cap, expected, timeout}, [&](const QByteArray &chunk) {
        delivered += chunk.size(); largestChunk = qMax(largestChunk, qint64(chunk.size()));
        if (cancelInSink) trustedTransport->cancel();
        return true;
      }, [&](TransferResult received) { result = received; ++completions; done = true; });
      QTRY_VERIFY_WITH_TIMEOUT(done, 7000);
      QCOMPARE(completions, 1);
      QCOMPARE(result.error.isEmpty(), success);
      QCOMPARE(server.requests, requestBaseline + 1);
      QVERIFY(delivered <= cap);
      QVERIFY(largestChunk <= 64 * 1024);
      QCOMPARE(result.bytes, delivered);
      if (success && expected >= 0) QCOMPARE(delivered, expected);
      if (!close) QTRY_VERIFY_WITH_TIMEOUT(server.disconnected > disconnectBaseline, 3000);
    };
    // A multi-chunk body traverses the actual network adapter without an all-body sink.
    const QByteArray payload(192 * 1024, 'x');
    runResponse("HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(payload.size()) +
                "\r\nConnection: close\r\n\r\n" + payload,
                payload.size(), payload.size(), true, false, true, 5000);
    // Header rejection occurs before any oversized body needs to arrive.
    runResponse("HTTP/1.1 200 OK\r\nContent-Length: 1000000\r\nConnection: close\r\n\r\n",
                32, -1, true, false, false, 5000);
    // Unknown-length chunked responses still meet the hard incremental cap.
    runResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n10\r\n0123456789abcdef\r\n0\r\n\r\n",
                8, -1, true, false, false, 5000);
    runResponse("HTTP/1.1 200 OK\r\nContent-Length: 10\r\nConnection: close\r\n\r\nabc",
                10, 10, true, false, false, 5000);
    runResponse("HTTP/1.1 200 OK\r\nContent-Length: 10\r\nConnection: close\r\n\r\n0123456789",
                20, 9, true, false, false, 5000);
    // Both explicit cancellation and the deadline abort a live partial response.
    runResponse("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nx",
                10, 10, false, true, false, 5000);
    runResponse("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nx",
                10, 10, false, false, false, 150);
    // Same-origin redirects are deliberately refused without requesting the target.
    runResponse("HTTP/1.1 302 Found\r\nLocation: /second\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
                32, -1, true, false, false, 5000);
  }
};

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  if ((argc == 3 || argc == 4) && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--update-child")) {
    const QString marker = QString::fromLocal8Bit(argv[2]);
    const int exitCode = argc == 4 ? QString::fromLocal8Bit(argv[3]).toInt() : 0;
    QTimer::singleShot(150, &application, [&application, marker, exitCode] {
      QFile file(marker);
      if (file.open(QIODevice::WriteOnly)) file.write("completed");
      application.exit(exitCode);
    });
    return application.exec();
  }
  UpdateServiceTest test;
  return QTest::qExec(&test, argc, argv);
}
#include "tst_update_service.moc"
