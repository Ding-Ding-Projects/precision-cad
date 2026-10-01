#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <functional>
#include <memory>

class QNetworkAccessManager;
class QSaveFile;
class QCryptographicHash;

namespace precision::update {
namespace staging { class Directory; }
enum class UpdateState { Idle, Checking, Available, Downloading, Ready, AwaitingApproval, Starting, Installing, Installed, Error, Unavailable };
struct UpdateInfo {
  QString version;
  QUrl notesUrl;
  QUrl packageUrl;
  QString sha256;
  QString unsignedWarning = QStringLiteral("This update is unsigned. Package hashes check integrity, not publisher authenticity.");
};
struct UpdateConfig {
  QString currentVersion;
  QUrl feedUrl;
  qint64 minimumFreeBytes = 64LL * 1024 * 1024;
};
struct TransferRequest {
  QUrl url;
  qint64 maximumBytes = 1024 * 1024;
  qint64 expectedBytes = -1;
  int timeoutMs = 30000;
};
struct TransferResult { int status = 0; QUrl finalUrl; qint64 bytes = 0; QString error; };
using TransferSink = std::function<bool(const QByteArray &)>;
class UpdateTransport {
public:
  virtual ~UpdateTransport() = default;
  virtual void get(const TransferRequest &, TransferSink, std::function<void(TransferResult)>) = 0;
  virtual void cancel() = 0;
};
class UpdateProcess {
public:
  virtual ~UpdateProcess() = default;
  virtual void start(const QString &, const QStringList &, std::function<void()> started,
                     std::function<void(QString)> completed) = 0;
};
// A supplied manager is an explicit test seam and must outlive the transport.
std::unique_ptr<UpdateTransport> makeQtUpdateTransport(QNetworkAccessManager *testManager = nullptr);
std::unique_ptr<UpdateProcess> makeQtUpdateProcess();

class UpdateService final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString state READ stateText NOTIFY stateChanged)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
  Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY updateChanged)
public:
  explicit UpdateService(UpdateConfig config, QObject *parent = nullptr);
  ~UpdateService() override;
  [[nodiscard]] UpdateState state() const noexcept { return m_state; }
  [[nodiscard]] QString stateText() const;
  [[nodiscard]] QString errorMessage() const { return m_error; }
  [[nodiscard]] QString availableVersion() const { return m_info.version; }
  [[nodiscard]] UpdateInfo updateInfo() const { return m_info; }
  [[nodiscard]] bool isInstalledSquirrelApplication() const;
  // Tests simulate an executable location, never an install root. Production uses applicationFilePath().
  void setExecutablePathForTesting(QString path);
  void setStagingDirectoryForTesting(QString path);
  void setTransport(std::unique_ptr<UpdateTransport> transport);
  void setProcess(std::unique_ptr<UpdateProcess> process);
  Q_INVOKABLE void startupCheck();
  Q_INVOKABLE void checkNow();
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void retry();
  Q_INVOKABLE void requestInstall();
  Q_INVOKABLE void approveInstall(const QString &approvalId, bool approved);
  static bool parseMetadata(const QByteArray &, const UpdateConfig &, UpdateInfo *, QString *);
  static bool verifyReleases(const QByteArray &, const UpdateInfo &, QString *);
  static bool isSafePackageName(const QString &);
  static bool isApprovedUrl(const QUrl &, const QUrl &);
signals:
  void stateChanged();
  void updateChanged();
  void restartApprovalRequested(const QString &approvalId, const QString &version);
private:
  void setState(UpdateState, QString error = {});
  void resetCandidate();
  void fail(QString error);
  void fetchMetadata();
  void fetchReleases();
  void downloadPackage();
  bool validateStaged() const;
  [[nodiscard]] QString installedRoot() const;
  [[nodiscard]] QString updateExePath() const;
  UpdateConfig m_config;
  QString m_testExecutable;
  QString m_testStagingDirectory;
  UpdateState m_state = UpdateState::Idle;
  UpdateInfo m_info;
  QString m_error, m_releaseSha1, m_approvalId;
  QByteArray m_metadata, m_releases, m_localRow;
  qint64 m_packageBytes = 0, m_receivedBytes = 0;
  quint64 m_generation = 0, m_approvalGeneration = 0;
  std::unique_ptr<staging::Directory> m_stage;
  std::unique_ptr<QSaveFile> m_packageFile;
  std::unique_ptr<QCryptographicHash> m_sha1, m_sha256;
  std::unique_ptr<UpdateTransport> m_transport;
  std::unique_ptr<UpdateProcess> m_process;
};
} // namespace precision::update
