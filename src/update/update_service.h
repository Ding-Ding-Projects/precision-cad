#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <functional>
#include <memory>

namespace precision::update {

enum class UpdateState { Idle, Checking, Available, Downloading, Ready, Installing, Error, Unavailable };

struct UpdateInfo {
  QString version;
  QUrl notesUrl;
  QUrl packageUrl;
  QString sha256;
  QString unsignedWarning = QStringLiteral("This update is unsigned and may show an operating-system warning.");
};

struct UpdateConfig {
  QString currentVersion;
  QUrl feedUrl;
  QString applicationDirectory;
  QString stagingDirectory;
  qint64 minimumFreeBytes = 64LL * 1024 * 1024;
};

struct TransferResult { int status = 0; QUrl finalUrl; QByteArray body; QString error; };
class UpdateTransport {
public:
  virtual ~UpdateTransport() = default;
  virtual void get(const QUrl &url, std::function<void(TransferResult)> completed) = 0;
};

class UpdateProcess {
public:
  virtual ~UpdateProcess() = default;
  virtual bool start(const QString &program, const QStringList &arguments, QString *error) = 0;
};

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
  void setTransport(std::unique_ptr<UpdateTransport> transport);
  void setProcess(std::unique_ptr<UpdateProcess> process);
  Q_INVOKABLE void startupCheck();
  Q_INVOKABLE void checkNow();
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void retry();
  Q_INVOKABLE void requestInstall();
  Q_INVOKABLE void approveInstall(bool approved);

  static bool parseMetadata(const QByteArray &json, const UpdateConfig &config, UpdateInfo *info, QString *error);
  static bool verifyReleases(const QByteArray &releases, const UpdateInfo &info, QString *error);
  static bool isSafePackageName(const QString &name);
  static bool isApprovedUrl(const QUrl &candidate, const QUrl &feed);
signals:
  void stateChanged();
  void updateChanged();
  void restartApprovalRequested();
private:
  void setState(UpdateState state, QString error = {});
  void fetchMetadata();
  void fetchReleases();
  void downloadPackage();
  void finishPackage(const TransferResult &result);
  [[nodiscard]] QString updateExePath() const;
  [[nodiscard]] QString releasesPath() const;
  UpdateConfig m_config;
  UpdateState m_state = UpdateState::Idle;
  UpdateInfo m_info;
  QString m_error;
  QString m_releaseSha1;
  quint64 m_generation = 0;
  std::unique_ptr<UpdateTransport> m_transport;
  std::unique_ptr<UpdateProcess> m_process;
};

} // namespace precision::update
