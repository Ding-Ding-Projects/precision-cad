#include "update_service.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QSaveFile>
#include <QStorageInfo>
#include <QVersionNumber>

namespace precision::update {
namespace {
constexpr qint64 kMaximumMetadataBytes = 1024 * 1024;
constexpr qint64 kMaximumPackageBytes = 2LL * 1024 * 1024 * 1024;

class QtTransport final : public UpdateTransport {
public:
  explicit QtTransport(QObject *parent) : m_manager(parent) {}
  void get(const QUrl &url, std::function<void(TransferResult)> completed) override {
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, completed = std::move(completed)] {
      TransferResult result;
      result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
      result.finalUrl = reply->url();
      result.body = reply->readAll();
      result.error = reply->error() == QNetworkReply::NoError ? QString{} : reply->errorString();
      reply->deleteLater();
      completed(std::move(result));
    });
  }
private:
  QNetworkAccessManager m_manager;
};

class QtProcess final : public UpdateProcess {
public:
  bool start(const QString &program, const QStringList &arguments, QString *error) override {
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(program, arguments);
    if (!process.waitForStarted(5000)) { if (error) *error = process.errorString(); return false; }
    return true;
  }
};

bool validSha256(const QString &value) {
  if (value.size() != 64) return false;
  for (const QChar c : value) if (!c.isDigit() && (c < u'a' || c > u'f') && (c < u'A' || c > u'F')) return false;
  return true;
}

QString releaseSha1(const QByteArray &releases, const UpdateInfo &info) {
  const QString wanted=QFileInfo(info.packageUrl.path()).fileName();
  for (const QByteArray &line : releases.split('\n')) {
    const QList<QByteArray> fields=line.trimmed().split(' '); if(fields.size()!=3) continue;
    const QString sha1=QString::fromLatin1(fields[0]), name=QString::fromUtf8(fields[1]);
    if (name == wanted && sha1.size() == 40 && fields[2].toLongLong() > 0 && UpdateService::isSafePackageName(name)) return sha1.toLower();
  }
  return {};
}

bool sameOrigin(const QUrl &left, const QUrl &right) {
  return left.scheme().compare(right.scheme(), Qt::CaseInsensitive) == 0
    && left.host().compare(right.host(), Qt::CaseInsensitive) == 0
    && left.port(left.scheme() == QStringLiteral("https") ? 443 : -1) == right.port(right.scheme() == QStringLiteral("https") ? 443 : -1);
}
}

UpdateService::UpdateService(UpdateConfig config, QObject *parent) : QObject(parent), m_config(std::move(config)), m_transport(std::make_unique<QtTransport>(this)), m_process(std::make_unique<QtProcess>()) {}
UpdateService::~UpdateService() = default;

QString UpdateService::stateText() const {
  switch (m_state) {
    case UpdateState::Idle: return tr("Idle"); case UpdateState::Checking: return tr("Checking for updates");
    case UpdateState::Available: return tr("Update available"); case UpdateState::Downloading: return tr("Downloading update");
    case UpdateState::Ready: return tr("Ready to restart"); case UpdateState::Installing: return tr("Waiting for restart approval");
    case UpdateState::Error: return tr("Update error"); case UpdateState::Unavailable: return tr("Updates unavailable in this build");
  }
  return tr("Unavailable");
}

QString UpdateService::updateExePath() const { return QDir(m_config.applicationDirectory).filePath(QStringLiteral("Update.exe")); }
QString UpdateService::releasesPath() const { return QDir(m_config.applicationDirectory).filePath(QStringLiteral("RELEASES")); }
bool UpdateService::isInstalledSquirrelApplication() const { return QFileInfo::exists(updateExePath()) && QFileInfo(releasesPath()).isFile(); }
void UpdateService::setTransport(std::unique_ptr<UpdateTransport> transport) { m_transport = std::move(transport); }
void UpdateService::setProcess(std::unique_ptr<UpdateProcess> process) { m_process = std::move(process); }
void UpdateService::setState(UpdateState state, QString error) { m_state=state; m_error=std::move(error); emit stateChanged(); }

void UpdateService::startupCheck() { if (m_state == UpdateState::Idle) checkNow(); }
void UpdateService::checkNow() {
  if (m_state == UpdateState::Checking || m_state == UpdateState::Downloading || m_state == UpdateState::Installing) return;
  if (!isInstalledSquirrelApplication()) { setState(UpdateState::Unavailable, tr("This standalone build is not installed by Squirrel.Windows.")); return; }
  if (!isApprovedUrl(m_config.feedUrl, m_config.feedUrl)) { setState(UpdateState::Error, tr("The declared update feed is not an approved HTTPS URL.")); return; }
  ++m_generation; m_info={}; m_releaseSha1.clear(); m_verifiedLocalFeed={}; setState(UpdateState::Checking); fetchMetadata();
}
void UpdateService::cancel() { if (m_state != UpdateState::Checking && m_state != UpdateState::Downloading) return; ++m_generation; m_info={}; setState(UpdateState::Idle); emit updateChanged(); }
void UpdateService::retry() { if (m_state == UpdateState::Error || m_state == UpdateState::Unavailable) checkNow(); }

bool UpdateService::isSafePackageName(const QString &name) {
  return !name.isEmpty() && name.size() <= 240 && !name.contains(QStringLiteral("..")) && !name.contains(u'/') && !name.contains(u'\\') && name.endsWith(QStringLiteral("-full.nupkg"), Qt::CaseInsensitive);
}
bool UpdateService::isApprovedUrl(const QUrl &candidate, const QUrl &feed) {
  return candidate.isValid() && candidate.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 && candidate.userInfo().isEmpty() && !candidate.host().isEmpty() && sameOrigin(candidate, feed);
}
bool UpdateService::parseMetadata(const QByteArray &json, const UpdateConfig &config, UpdateInfo *info, QString *error) {
  if (json.size() > kMaximumMetadataBytes) { if(error) *error=QStringLiteral("Update metadata exceeds the bounded size."); return false; }
  QJsonParseError parseError; const auto doc=QJsonDocument::fromJson(json,&parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) { if(error) *error=QStringLiteral("Update metadata is not valid JSON."); return false; }
  const auto object=doc.object();
  const QString version=object.value(QStringLiteral("version")).toString(), sha=object.value(QStringLiteral("sha256")).toString();
  const QUrl package=QUrl(object.value(QStringLiteral("packageUrl")).toString()), notes=QUrl(object.value(QStringLiteral("notesUrl")).toString());
  if (version.isEmpty() || QVersionNumber::fromString(version).isNull() || QVersionNumber::compare(QVersionNumber::fromString(version),QVersionNumber::fromString(config.currentVersion)) <= 0) { if(error) *error=QStringLiteral("Update version is missing, invalid, or not newer."); return false; }
  if (!validSha256(sha) || !isApprovedUrl(package,config.feedUrl) || !isApprovedUrl(notes,config.feedUrl) || !isSafePackageName(QFileInfo(package.path()).fileName())) { if(error) *error=QStringLiteral("Update metadata contains an unsafe URL, package name, or hash."); return false; }
  if (info) *info={version,notes,package,sha.toLower(),QStringLiteral("This update is unsigned and may show an operating-system warning.")}; return true;
}
bool UpdateService::verifyReleases(const QByteArray &releases, const UpdateInfo &info, QString *error) {
  if (!releaseSha1(releases, info).isEmpty()) return true;
  if(error) *error=QStringLiteral("The Squirrel RELEASES manifest does not index the candidate full package safely."); return false;
}
void UpdateService::fetchMetadata() {
  const quint64 generation=m_generation; m_transport->get(m_config.feedUrl,[this,generation](TransferResult result) {
    if(generation!=m_generation) return;
    if(!result.error.isEmpty() || result.status!=200 || !isApprovedUrl(result.finalUrl,m_config.feedUrl)) { setState(UpdateState::Error,tr("The update feed request was refused.")); return; }
    QString error; if(!parseMetadata(result.body,m_config,&m_info,&error)) { setState(UpdateState::Error,error); return; }
    emit updateChanged(); setState(UpdateState::Available); fetchReleases();
  });
}
void UpdateService::fetchReleases() {
  const QUrl releases=m_config.feedUrl.resolved(QUrl(QStringLiteral("RELEASES"))); const quint64 generation=m_generation;
  m_transport->get(releases,[this,generation,releases](TransferResult result) {
    if(generation!=m_generation) return;
    QString error; if(!result.error.isEmpty() || result.status!=200 || !isApprovedUrl(result.finalUrl,releases) || !verifyReleases(result.body,m_info,&error)) { setState(UpdateState::Error,error.isEmpty()?tr("The Squirrel RELEASES manifest was refused."):error); return; }
    m_releaseSha1=releaseSha1(result.body,m_info);
    downloadPackage();
  });
}
void UpdateService::downloadPackage() {
  setState(UpdateState::Downloading); const quint64 generation=m_generation;
  m_transport->get(m_info.packageUrl,[this,generation](TransferResult result) { if(generation==m_generation) finishPackage(result); });
}
void UpdateService::finishPackage(const TransferResult &result) {
  if(!result.error.isEmpty() || result.status!=200 || !isApprovedUrl(result.finalUrl,m_config.feedUrl) || result.body.size()<=0 || result.body.size()>kMaximumPackageBytes) { setState(UpdateState::Error,tr("The update package download was refused.")); return; }
  const QString digest=QString::fromLatin1(QCryptographicHash::hash(result.body,QCryptographicHash::Sha256).toHex());
  const QString sha1=QString::fromLatin1(QCryptographicHash::hash(result.body,QCryptographicHash::Sha1).toHex());
  if(digest!=m_info.sha256 || sha1!=m_releaseSha1) { setState(UpdateState::Error,tr("The downloaded update package hash did not match metadata or Squirrel RELEASES.")); return; }
  QDir stage(m_config.stagingDirectory); if(!stage.exists() && !stage.mkpath(QStringLiteral("."))) { setState(UpdateState::Error,tr("The update staging directory could not be created.")); return; }
  if(QStorageInfo(stage.absolutePath()).bytesAvailable()<qMax(m_config.minimumFreeBytes,qint64(result.body.size())*2)) { setState(UpdateState::Error,tr("There is insufficient disk space to stage the update.")); return; }
  const QString packageName=QFileInfo(m_info.packageUrl.path()).fileName();
  QSaveFile file(stage.filePath(packageName)); if(!file.open(QIODevice::WriteOnly) || file.write(result.body)!=result.body.size() || !file.commit()) { setState(UpdateState::Error,tr("The update package could not be staged atomically.")); return; }
  QSaveFile manifest(stage.filePath(QStringLiteral("RELEASES")));
  const QByteArray localRow=m_releaseSha1.toLatin1()+" "+packageName.toUtf8()+" "+QByteArray::number(result.body.size())+"\n";
  if(!manifest.open(QIODevice::WriteOnly) || manifest.write(localRow)!=localRow.size() || !manifest.commit()) { setState(UpdateState::Error,tr("The verified local Squirrel feed could not be staged atomically.")); return; }
  m_verifiedLocalFeed=QUrl::fromLocalFile(stage.absolutePath()+QDir::separator());
  setState(UpdateState::Ready);
}
void UpdateService::requestInstall() {
  if(m_state!=UpdateState::Ready) return;
  setState(UpdateState::Installing); emit restartApprovalRequested();
}
void UpdateService::approveInstall(bool approved) {
  if(m_state!=UpdateState::Installing) return;
  if(!approved) { setState(UpdateState::Ready); return; }
  if(!m_verifiedLocalFeed.isLocalFile()) { setState(UpdateState::Error,tr("No verified local Squirrel feed is available.")); return; }
  // The shell has checked unsaved work. The mutable HTTPS feed is never fetched again after verification.
  QString error; if(!m_process->start(updateExePath(),{QStringLiteral("--update=%1").arg(m_verifiedLocalFeed.toString(QUrl::FullyEncoded))},&error)) setState(UpdateState::Error,tr("Squirrel Update.exe could not start: %1").arg(error));
}
} // namespace precision::update
