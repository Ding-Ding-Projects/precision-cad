#include "update_service.h"
#include "private_staging.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QVersionNumber>

namespace precision::update {
namespace {
constexpr qint64 kMaximumMetadataBytes = 1024 * 1024;
constexpr qint64 kMaximumPackageBytes = 2LL * 1024 * 1024 * 1024;
constexpr qint64 kChunkBytes = 64 * 1024;

struct ReleaseRow { QString sha1, name; qint64 bytes = 0; };
bool hexDigest(const QString &value, int length) {
  if (value.size() != length) return false;
  for (const QChar c : value)
    if (!((c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F'))) return false;
  return true;
}
bool releaseRow(const QByteArray &manifest, const QString &wanted, ReleaseRow *out) {
  if (manifest.isEmpty() || manifest.size() > kMaximumMetadataBytes) return false;
  int matches = 0;
  for (const auto &line : manifest.split('\n')) {
    const auto fields = line.trimmed().split(' ');
    if (fields.size() != 3 || QString::fromUtf8(fields[1]) != wanted) continue;
    bool ok = false;
    const qint64 bytes = fields[2].toLongLong(&ok);
    const QString hash = QString::fromLatin1(fields[0]);
    if (!ok || bytes <= 0 || bytes > kMaximumPackageBytes || !hexDigest(hash, 40)
        || !UpdateService::isSafePackageName(wanted)) return false;
    ++matches;
    if (out) *out = {hash.toLower(), wanted, bytes};
  }
  return matches == 1;
}
bool hashFile(const QString &path, qint64 expected, QString *sha1, QString *sha256) {
  const QFileInfo info(path);
  if (!info.isFile() || info.isSymLink() || info.size() != expected || expected <= 0 || expected > kMaximumPackageBytes) return false;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return false;
  QCryptographicHash first(QCryptographicHash::Sha1), second(QCryptographicHash::Sha256);
  qint64 bytes = 0;
  while (!file.atEnd()) {
    const QByteArray chunk = file.read(kChunkBytes);
    if (chunk.isEmpty() || chunk.size() > expected - bytes) return false;
    bytes += chunk.size();
    first.addData(chunk); second.addData(chunk);
  }
  if (file.error() != QFileDevice::NoError || bytes != expected) return false;
  if (sha1) *sha1 = QString::fromLatin1(first.result().toHex());
  if (sha256) *sha256 = QString::fromLatin1(second.result().toHex());
  return true;
}
QByteArray boundedFile(const QString &path) {
  const QFileInfo info(path);
  if (!info.isFile() || info.isSymLink() || info.size() > kMaximumMetadataBytes) return {};
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  const QByteArray bytes = file.read(kMaximumMetadataBytes + 1);
  return bytes.size() <= kMaximumMetadataBytes && file.error() == QFileDevice::NoError ? bytes : QByteArray{};
}

class QtTransport final : public QObject, public UpdateTransport {
public:
  explicit QtTransport(QNetworkAccessManager *manager) : m_manager(manager) {
    if (!m_manager) m_manager = new QNetworkAccessManager(this);
  }
  ~QtTransport() override {
    if (m_reply) {
      QObject::disconnect(m_reply, nullptr, this, nullptr);
      m_reply->abort();
      m_reply->deleteLater();
    }
  }
  void cancel() override {
    if (!m_reply) return;
    m_failure = QStringLiteral("Update transfer cancelled.");
    m_reply->abort();
  }
  void get(const TransferRequest &spec, TransferSink sink, std::function<void(TransferResult)> completed) override {
    if (m_reply) {
      completed({0, spec.url, 0, QStringLiteral("Another update transfer is active.")});
      return;
    }
    if (!UpdateService::isApprovedUrl(spec.url, spec.url) || spec.maximumBytes <= 0
        || spec.maximumBytes > kMaximumPackageBytes || spec.expectedBytes > spec.maximumBytes
        || spec.expectedBytes < -1 || spec.timeoutMs <= 0) {
      completed({0, spec.url, 0, QStringLiteral("Invalid bounded HTTPS request.")});
      return;
    }
    m_spec = spec; m_sink = std::move(sink); m_completed = std::move(completed);
    m_bytes = 0; m_failure.clear();
    QNetworkRequest request(spec.url);
    // Redirects are deliberately refused, including same-origin ones, before requesting another URL.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept-Encoding", "identity");
    request.setTransferTimeout(spec.timeoutMs);
    m_reply = m_manager->get(request);
    m_reply->setReadBufferSize(kChunkBytes);
    auto *deadline = new QTimer(m_reply);
    deadline->setSingleShot(true);
    QNetworkReply *requestReply = m_reply.data();
    QObject::connect(deadline, &QTimer::timeout, this, [this, requestReply] {
      if (m_reply == requestReply) { m_failure = QStringLiteral("Update transfer deadline exceeded."); m_reply->abort(); }
    });
    deadline->start(spec.timeoutMs);
    QObject::connect(m_reply, &QNetworkReply::metaDataChanged, this, [this] { validateHeaders(); });
    QObject::connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
      if (!m_reply) return;
      if (received > m_spec.maximumBytes || total > m_spec.maximumBytes
          || (m_spec.expectedBytes >= 0 && (received > m_spec.expectedBytes || (total >= 0 && total != m_spec.expectedBytes)))) {
        m_failure = QStringLiteral("Update response exceeds or differs from its declared byte count.");
        m_reply->abort();
      }
    });
    QObject::connect(m_reply, &QNetworkReply::readyRead, this, [this] { drain(); });
    QObject::connect(m_reply, &QNetworkReply::finished, this, [this, deadline, requestReply] {
      if (m_reply != requestReply) return;
      deadline->stop();
      auto *reply = m_reply.data();
      if (m_failure.isEmpty() && reply->error() == QNetworkReply::NoError) drain();
      if (m_reply != requestReply) return;
      TransferResult result{reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), reply->url(), m_bytes, m_failure};
      if (result.error.isEmpty() && reply->error() != QNetworkReply::NoError) result.error = reply->errorString();
      if (result.error.isEmpty() && (result.status != 200 || !UpdateService::isApprovedUrl(result.finalUrl, m_spec.url)))
        result.error = QStringLiteral("Update response status or origin was refused.");
      if (result.error.isEmpty() && m_spec.expectedBytes >= 0 && m_bytes != m_spec.expectedBytes)
        result.error = QStringLiteral("Update response does not match its declared byte count.");
      auto completed = std::move(m_completed);
      m_sink = {}; m_reply = nullptr;
      QObject::disconnect(reply, nullptr, this, nullptr);
      reply->deleteLater();
      completed(std::move(result));
    });
  }
private:
  bool validateHeaders() {
    if (!m_reply || !m_failure.isEmpty()) return false;
    const int status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QVariant length = m_reply->header(QNetworkRequest::ContentLengthHeader);
    bool ok = false;
    const qint64 count = length.toLongLong(&ok);
    const auto encoding = m_reply->rawHeader("Content-Encoding").trimmed().toLower();
    if ((status && status != 200) || (!encoding.isEmpty() && encoding != "identity")
        || (length.isValid() && (!ok || count < 0 || count > m_spec.maximumBytes
            || (m_spec.expectedBytes >= 0 && count != m_spec.expectedBytes)))) {
      m_failure = QStringLiteral("Update response headers were refused.");
      m_reply->abort(); return false;
    }
    return true;
  }
  void drain() {
    if (!validateHeaders()) return;
    const QPointer<QNetworkReply> original = m_reply;
    while (m_reply && m_reply == original && m_reply->bytesAvailable() > 0) {
      const qint64 remaining = qMin(m_spec.maximumBytes, m_spec.expectedBytes >= 0 ? m_spec.expectedBytes : m_spec.maximumBytes) - m_bytes;
      if (remaining <= 0) {
        m_failure = QStringLiteral("Update response exceeded the bounded byte count.");
        m_reply->abort(); return;
      }
      const QByteArray chunk = m_reply->read(qMin(kChunkBytes, remaining));
      if (chunk.isEmpty()) return;
      m_bytes += chunk.size();
      const auto sink = m_sink;
      const bool accepted = sink(chunk);
      if (m_reply != original) return;
      if (!accepted && m_reply) {
        m_failure = QStringLiteral("Update staging refused a response chunk.");
        m_reply->abort(); return;
      }
    }
  }
  QNetworkAccessManager *m_manager;
  QPointer<QNetworkReply> m_reply;
  TransferRequest m_spec;
  TransferSink m_sink;
  std::function<void(TransferResult)> m_completed;
  QString m_failure;
  qint64 m_bytes = 0;
};

class QtProcess final : public QObject, public UpdateProcess {
public:
  ~QtProcess() override {
    if (!m_process) return;
    // Closing the service must not terminate an already-authorized installation.
    auto *process = m_process;
    QObject::disconnect(process, nullptr, this, nullptr);
    process->setParent(QCoreApplication::instance());
    QObject::connect(process, &QProcess::finished, process, &QObject::deleteLater);
    QObject::connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
      if (error == QProcess::FailedToStart) process->deleteLater();
    });
    if (process->state() == QProcess::NotRunning) process->deleteLater();
  }
  void start(const QString &program, const QStringList &arguments, std::function<void()> started,
             std::function<void(QString)> completed) override {
    if (m_process) { completed(QStringLiteral("The updater process is already running.")); return; }
    m_process = new QProcess(this);
    auto *process = m_process;
    // Drain output without retaining installation paths or allowing unbounded buffering.
    process->setStandardOutputFile(QProcess::nullDevice());
    process->setStandardErrorFile(QProcess::nullDevice());
    auto finish = [this, process, completed = std::move(completed)](QString error) mutable {
      if (m_process != process) return;
      m_process = nullptr;
      QObject::disconnect(process, nullptr, this, nullptr);
      process->deleteLater();
      completed(std::move(error));
    };
    QObject::connect(process, &QProcess::started, this, [started = std::move(started)] { started(); });
    QObject::connect(process, &QProcess::errorOccurred, this, [finish](QProcess::ProcessError error) mutable {
      if (error == QProcess::FailedToStart) finish(QStringLiteral("Squirrel Update.exe could not start."));
    });
    QObject::connect(process, &QProcess::finished, this, [finish](int code, QProcess::ExitStatus status) mutable {
      finish(status == QProcess::NormalExit && code == 0 ? QString{} :
             QStringLiteral("Squirrel Update.exe failed with exit code %1.").arg(code));
    });
    process->start(program, arguments);
  }
private:
  QProcess *m_process = nullptr;
};
} // namespace

std::unique_ptr<UpdateTransport> makeQtUpdateTransport(QNetworkAccessManager *manager) { return std::make_unique<QtTransport>(manager); }
std::unique_ptr<UpdateProcess> makeQtUpdateProcess() { return std::make_unique<QtProcess>(); }

UpdateService::UpdateService(UpdateConfig config, QObject *parent)
  : QObject(parent), m_config(std::move(config)), m_transport(makeQtUpdateTransport()), m_process(makeQtUpdateProcess()) {}
UpdateService::~UpdateService() {
  ++m_generation;
  m_transport.reset();
  // Preserve the feed if the owner closes during an authorized installation.
  // A later owner may clean this orphan; it must not disappear under the child.
  if (m_stage && (m_state == UpdateState::Starting || m_state == UpdateState::Installing)) m_stage->setAutoRemove(false);
  m_process.reset();
}
QString UpdateService::stateText() const {
  switch (m_state) {
    case UpdateState::Idle: return tr("Idle");
    case UpdateState::Checking: return tr("Checking for updates");
    case UpdateState::Available: return tr("Update available");
    case UpdateState::Downloading: return tr("Downloading update");
    case UpdateState::Ready: return tr("Update ready");
    case UpdateState::AwaitingApproval: return tr("Waiting for installation approval");
    case UpdateState::Starting: return tr("Starting update installation");
    case UpdateState::Installing: return tr("Installing update");
    case UpdateState::Installed: return tr("Update installed; restart required");
    case UpdateState::Error: return tr("Update error");
    case UpdateState::Unavailable: return tr("Updates unavailable in this build");
  }
  return tr("Unavailable");
}
QString UpdateService::installedRoot() const {
  const QFileInfo executable(m_testExecutable.isEmpty() ? QCoreApplication::applicationFilePath() : m_testExecutable);
  if (!executable.isFile() || executable.isSymLink()) return {};
  const QDir app = executable.absoluteDir();
  if (app.dirName() != QStringLiteral("app-%1").arg(m_config.currentVersion) || QFileInfo(app.absolutePath()).isSymLink()) return {};
  QDir root = app; if (!root.cdUp()) return {};
  const QFileInfo updater(root.filePath(QStringLiteral("Update.exe")));
  if (!updater.isFile() || updater.isSymLink()) return {};
  const QString name = QStringLiteral("PrecisionCAD-%1-full.nupkg").arg(m_config.currentVersion);
  const QString packages = root.filePath(QStringLiteral("packages"));
  if (QFileInfo(packages).isSymLink()) return {};
  ReleaseRow row;
  if (!releaseRow(boundedFile(QDir(packages).filePath(QStringLiteral("RELEASES"))), name, &row)) return {};
  QString hash;
  if (!hashFile(QDir(packages).filePath(name), row.bytes, &hash, nullptr) || hash != row.sha1) return {};
  return root.canonicalPath();
}
QString UpdateService::updateExePath() const {
  const QString root = installedRoot();
  return root.isEmpty() ? QString{} : QDir(root).filePath(QStringLiteral("Update.exe"));
}
bool UpdateService::isInstalledSquirrelApplication() const { return !installedRoot().isEmpty(); }
void UpdateService::setExecutablePathForTesting(QString path) { if (m_state == UpdateState::Idle) m_testExecutable = std::move(path); }
void UpdateService::setStagingDirectoryForTesting(QString path) { if (m_state == UpdateState::Idle) m_testStagingDirectory = std::move(path); }
void UpdateService::setTransport(std::unique_ptr<UpdateTransport> transport) { if (m_state == UpdateState::Idle && transport) m_transport = std::move(transport); }
void UpdateService::setProcess(std::unique_ptr<UpdateProcess> process) { if (m_state == UpdateState::Idle && process) m_process = std::move(process); }
void UpdateService::setState(UpdateState state, QString error) { m_state = state; m_error = std::move(error); emit stateChanged(); }
void UpdateService::resetCandidate() {
  m_approvalId.clear(); m_approvalGeneration = 0;
  m_packageFile.reset(); m_sha1.reset(); m_sha256.reset(); m_stage.reset();
  m_metadata.clear(); m_releases.clear(); m_localRow.clear(); m_releaseSha1.clear();
  m_packageBytes = 0; m_receivedBytes = 0; m_info = {};
}
void UpdateService::fail(QString error) { resetCandidate(); emit updateChanged(); setState(UpdateState::Error, std::move(error)); }
void UpdateService::startupCheck() { if (m_state == UpdateState::Idle) checkNow(); }
void UpdateService::checkNow() {
  if (m_state == UpdateState::Checking || m_state == UpdateState::Available || m_state == UpdateState::Downloading
      || m_state == UpdateState::Starting || m_state == UpdateState::Installing || m_state == UpdateState::Installed) return;
  ++m_generation; m_transport->cancel(); resetCandidate(); emit updateChanged();
  if (!isInstalledSquirrelApplication()) { setState(UpdateState::Unavailable, tr("The running executable has no verified Squirrel.Windows installation.")); return; }
  if (!isApprovedUrl(m_config.feedUrl, m_config.feedUrl)) { fail(tr("The update feed is not an approved HTTPS URL.")); return; }
  setState(UpdateState::Checking); fetchMetadata();
}
void UpdateService::cancel() {
  if (m_state != UpdateState::Checking && m_state != UpdateState::Available && m_state != UpdateState::Downloading
      && m_state != UpdateState::Ready && m_state != UpdateState::AwaitingApproval) return;
  ++m_generation; m_transport->cancel(); resetCandidate(); emit updateChanged(); setState(UpdateState::Idle);
}
void UpdateService::retry() { if (m_state == UpdateState::Error || m_state == UpdateState::Unavailable) checkNow(); }
bool UpdateService::isSafePackageName(const QString &name) {
  static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,228}-full[.]nupkg$"));
  return !name.contains(QStringLiteral("..")) && pattern.match(name).hasMatch();
}
bool UpdateService::isApprovedUrl(const QUrl &candidate, const QUrl &feed) {
  return candidate.isValid() && candidate.scheme() == QStringLiteral("https") && candidate.userInfo().isEmpty()
    && !candidate.host().isEmpty() && candidate.fragment().isEmpty()
    && candidate.host().compare(feed.host(), Qt::CaseInsensitive) == 0 && candidate.port(443) == feed.port(443)
    && feed.scheme() == QStringLiteral("https");
}
bool UpdateService::parseMetadata(const QByteArray &json, const UpdateConfig &config, UpdateInfo *info, QString *error) {
  auto reject = [error](const QString &message) { if (error) *error = message; return false; };
  if (json.size() > kMaximumMetadataBytes) return reject(QStringLiteral("Update metadata exceeds its bounded size."));
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(json, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) return reject(QStringLiteral("Update metadata is not valid JSON."));
  const auto object = document.object();
  const QString version = object.value(QStringLiteral("version")).toString();
  const QString sha = object.value(QStringLiteral("sha256")).toString();
  const QUrl package(object.value(QStringLiteral("packageUrl")).toString()), notes(object.value(QStringLiteral("notesUrl")).toString());
  qsizetype suffix = 0, currentSuffix = 0;
  const auto candidate = QVersionNumber::fromString(version, &suffix), current = QVersionNumber::fromString(config.currentVersion, &currentSuffix);
  if (candidate.isNull() || suffix != version.size() || current.isNull() || currentSuffix != config.currentVersion.size()
      || QVersionNumber::compare(candidate, current) <= 0) return reject(QStringLiteral("Update version is missing, invalid, or not newer."));
  const QString wanted = QStringLiteral("PrecisionCAD-%1-full.nupkg").arg(version);
  if (!hexDigest(sha, 64) || !isApprovedUrl(package, config.feedUrl) || !isApprovedUrl(notes, config.feedUrl)
      || QFileInfo(package.path()).fileName() != wanted || !isSafePackageName(wanted))
    return reject(QStringLiteral("Update metadata contains an unsafe URL, package identity, or hash."));
  if (info) *info = {version, notes, package, sha.toLower()};
  return true;
}
bool UpdateService::verifyReleases(const QByteArray &bytes, const UpdateInfo &info, QString *error) {
  if (releaseRow(bytes, QFileInfo(info.packageUrl.path()).fileName(), nullptr)) return true;
  if (error) *error = QStringLiteral("Squirrel RELEASES must index exactly one valid candidate full package.");
  return false;
}
void UpdateService::fetchMetadata() {
  const quint64 generation = m_generation;
  m_transport->get({m_config.feedUrl, kMaximumMetadataBytes, -1, 30000},
    [this, generation](const QByteArray &chunk) {
      if (generation != m_generation || chunk.size() > kMaximumMetadataBytes - m_metadata.size()) return false;
      m_metadata.append(chunk); return true;
    },
    [this, generation](TransferResult result) {
      if (generation != m_generation) return;
      if (!result.error.isEmpty() || result.status != 200 || !isApprovedUrl(result.finalUrl, m_config.feedUrl)) { fail(tr("The update feed request was refused.")); return; }
      QString error;
      if (!parseMetadata(m_metadata, m_config, &m_info, &error)) { fail(error); return; }
      m_metadata.clear(); emit updateChanged(); setState(UpdateState::Available);
      if (generation == m_generation) fetchReleases();
    });
}
void UpdateService::fetchReleases() {
  const quint64 generation = m_generation;
  const QUrl url = m_config.feedUrl.resolved(QUrl(QStringLiteral("RELEASES")));
  m_transport->get({url, kMaximumMetadataBytes, -1, 30000},
    [this, generation](const QByteArray &chunk) {
      if (generation != m_generation || chunk.size() > kMaximumMetadataBytes - m_releases.size()) return false;
      m_releases.append(chunk); return true;
    },
    [this, generation, url](TransferResult result) {
      if (generation != m_generation) return;
      ReleaseRow row;
      if (!result.error.isEmpty() || result.status != 200 || !isApprovedUrl(result.finalUrl, url)
          || !releaseRow(m_releases, QFileInfo(m_info.packageUrl.path()).fileName(), &row)) {
        fail(tr("The Squirrel RELEASES manifest was refused.")); return;
      }
      m_releaseSha1 = row.sha1; m_packageBytes = row.bytes;
      m_localRow = row.sha1.toLatin1() + " " + row.name.toUtf8() + " " + QByteArray::number(row.bytes) + "\n";
      m_releases.clear(); downloadPackage();
    });
}
void UpdateService::downloadPackage() {
  const QString applicationData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  const QString root = m_testStagingDirectory.isEmpty()
    ? (applicationData.isEmpty() ? QString{} : QDir(applicationData).filePath(QStringLiteral("updates"))) : m_testStagingDirectory;
  QDir base(root);
  if (!staging::prepareRoot(root)) {
    fail(tr("Update staging requires a local, current-user-owned directory with verified private access permissions.")); return;
  }
  if (QStorageInfo(base.absolutePath()).bytesAvailable() < qMax(m_config.minimumFreeBytes, m_packageBytes * 2)) {
    fail(tr("There is insufficient disk space to stage the update.")); return;
  }
  m_stage = std::make_unique<staging::Directory>(base.absolutePath());
  if (!m_stage->isValid()) { fail(tr("A unique update staging directory could not be created.")); return; }
  m_packageFile = std::make_unique<QSaveFile>(QDir(m_stage->path()).filePath(QFileInfo(m_info.packageUrl.path()).fileName()));
  m_packageFile->setDirectWriteFallback(false);
  if (!m_packageFile->open(QIODevice::WriteOnly)) { fail(tr("The update package could not be staged.")); return; }
  m_sha1 = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha1);
  m_sha256 = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);
  m_receivedBytes = 0;
  const quint64 generation = m_generation;
  setState(UpdateState::Downloading);
  if (generation != m_generation) return;
  m_transport->get({m_info.packageUrl, m_packageBytes, m_packageBytes, 300000},
    [this, generation](const QByteArray &chunk) {
      if (generation != m_generation || !m_packageFile || chunk.size() > m_packageBytes - m_receivedBytes) return false;
      if (m_packageFile->write(chunk) != chunk.size()) return false;
      m_receivedBytes += chunk.size(); m_sha1->addData(chunk); m_sha256->addData(chunk); return true;
    },
    [this, generation](TransferResult result) {
      if (generation != m_generation) return;
      if (!result.error.isEmpty() || result.status != 200 || !isApprovedUrl(result.finalUrl, m_config.feedUrl)
          || m_receivedBytes != m_packageBytes || result.bytes != m_packageBytes
          || QString::fromLatin1(m_sha1->result().toHex()) != m_releaseSha1
          || QString::fromLatin1(m_sha256->result().toHex()) != m_info.sha256) {
        fail(tr("The downloaded package did not match its declared size and integrity hashes.")); return;
      }
      if (!m_packageFile->commit()) { fail(tr("The package could not be committed atomically.")); return; }
      m_packageFile.reset();
      QSaveFile manifest(QDir(m_stage->path()).filePath(QStringLiteral("RELEASES")));
      manifest.setDirectWriteFallback(false);
      if (!manifest.open(QIODevice::WriteOnly) || manifest.write(m_localRow) != m_localRow.size() || !manifest.commit()) {
        fail(tr("The verified local feed could not be staged atomically.")); return;
      }
      setState(UpdateState::Ready);
    });
}
bool UpdateService::validateStaged() const {
  if (!m_stage || !m_stage->isValid() || !staging::verifyPrivateDirectory(m_stage->path())
      || !staging::verifyPrivateDirectory(QFileInfo(m_stage->path()).absolutePath())) return false;
  const QDir stage(m_stage->path());
  if (stage.entryList(QDir::Files | QDir::Hidden | QDir::System).size() != 2
      || stage.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System).size() != 0
      || boundedFile(stage.filePath(QStringLiteral("RELEASES"))) != m_localRow) return false;
  QString sha1, sha256;
  return hashFile(stage.filePath(QFileInfo(m_info.packageUrl.path()).fileName()), m_packageBytes, &sha1, &sha256)
    && sha1 == m_releaseSha1 && sha256 == m_info.sha256;
}
void UpdateService::requestInstall() {
  if (m_state != UpdateState::Ready) return;
  m_approvalId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  m_approvalGeneration = m_generation;
  const QString approval = m_approvalId, version = m_info.version;
  setState(UpdateState::AwaitingApproval);
  if (m_state == UpdateState::AwaitingApproval && m_approvalId == approval) emit restartApprovalRequested(approval, version);
}
void UpdateService::approveInstall(const QString &approvalId, bool approved) {
  if (m_state != UpdateState::AwaitingApproval || approvalId.isEmpty() || approvalId != m_approvalId
      || m_approvalGeneration != m_generation) return;
  m_approvalId.clear();
  if (!approved) { setState(UpdateState::Ready); return; }
  const QString updater = updateExePath();
  if (updater.isEmpty() || !validateStaged()) { fail(tr("The installed updater or staged candidate changed before approval.")); return; }
  // Squirrel 1.9.1 expects an absolute native directory, not a file URI.
  const QString localFeed = QDir::toNativeSeparators(QDir(m_stage->path()).absolutePath());
  const quint64 generation = m_generation;
  setState(UpdateState::Starting);
  m_process->start(updater, {QStringLiteral("--update=%1").arg(localFeed)},
    [this, generation] { if (generation == m_generation) setState(UpdateState::Installing); },
    [this, generation](QString error) {
      if (generation != m_generation) return;
      if (!error.isEmpty()) { fail(error); return; }
      resetCandidate(); emit updateChanged(); setState(UpdateState::Installed);
    });
}
} // namespace precision::update
