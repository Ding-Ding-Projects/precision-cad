#include "native_diagnostics.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QSaveFile>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace precision::diagnostics {
namespace {
// JSON numbers remain exact in every consumer, including JavaScript.
constexpr quint64 kMaximumCount = 9007199254740991ULL;
struct Counts {
  quint64 qtWarnings = 0, qtCriticals = 0, qtFatals = 0, qmlWarnings = 0;
  bool overflow = false;
  bool reentrant = false;
  bool accepting = true;
};
struct Dispatcher {
  std::mutex mutex;
  std::mutex startupMutex;
  std::shared_ptr<Counts> active;
  std::atomic<QtMessageHandler> previous{nullptr};
  std::once_flag installation;
};
// Later handlers may retain our callback. Never restore by temporarily replacing
// the process-global handler, and never destroy this forwarding node.
Dispatcher &dispatcher() { static auto *value = new Dispatcher; return *value; }
QString utcNow() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
void increment(quint64 &value, quint64 amount, bool &overflow) {
  if (amount > kMaximumCount - value) { value = kMaximumCount; overflow = true; }
  else value += amount;
}
bool validSource(const QString &source) {
  if (source.size() != 40) return false;
  for (const QChar ch : source)
    if (!(ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) &&
        !(ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))) return false;
  return true;
}
bool safeExistingAncestors(const QString &absolute) {
  QString cursor = absolute;
  for (;;) {
#ifdef Q_OS_WIN
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(cursor.utf16()));
    if (attributes != INVALID_FILE_ATTRIBUTES) {
      if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
    } else {
      const DWORD error = GetLastError();
      if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return false;
    }
#else
    const QFileInfo part(cursor);
    if (part.isSymLink() || (part.exists() && !part.isDir())) return false;
#endif
    const QString parent = QFileInfo(cursor).absolutePath();
    if (parent == cursor) break;
    cursor = parent;
  }
  return true;
}
bool createPrivateDirectory(const QString &path) {
#ifdef Q_OS_WIN
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
  DWORD size = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  std::vector<unsigned char> buffer(size);
  const bool gotUser = size && GetTokenInformation(token, TokenUser, buffer.data(), size, &size);
  CloseHandle(token);
  if (!gotUser) return false;
  LPWSTR sid = nullptr;
  if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(buffer.data())->User.Sid, &sid)) return false;
  const QString sddl = QStringLiteral("D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;") + QString::fromWCharArray(sid) + QStringLiteral(")");
  LocalFree(sid);
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(reinterpret_cast<LPCWSTR>(sddl.utf16()), SDDL_REVISION_1, &descriptor, nullptr)) return false;
  SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
  const bool created = CreateDirectoryW(reinterpret_cast<LPCWSTR>(path.utf16()), &security);
  LocalFree(descriptor);
  return created;
#else
  return ::mkdir(QFile::encodeName(path).constData(), S_IRWXU) == 0;
#endif
}
bool atomicWrite(const QString &path, const QJsonObject &record) {
  if (!safeExistingAncestors(QFileInfo(path).absolutePath())) return false;
#ifdef Q_OS_WIN
  const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
  if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) return false;
#else
  if (QFileInfo(path).isSymLink()) return false;
#endif
  const QByteArray payload = QJsonDocument(record).toJson(QJsonDocument::Compact);
  QSaveFile output(path);
  output.setDirectWriteFallback(false);
  if (!output.open(QIODevice::WriteOnly) || output.write(payload) != payload.size() || !output.flush()) return false;
#ifdef Q_OS_WIN
  const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(output.handle()));
  if (handle == INVALID_HANDLE_VALUE || !FlushFileBuffers(handle)) return false;
#else
  if (::fsync(output.handle()) != 0) return false;
#endif
  return output.commit();
}
} // namespace

struct NativeDiagnosticsCollector::State {
  QString runDirectory, sourceCommit, id, startedAt;
  std::shared_ptr<Counts> counts = std::make_shared<Counts>();
  quint64 sequence = 0;
  bool writeFailed = false;
  bool registered = false;
  bool finalized = false;
  bool sealed = false;
};

bool hasValidatedAuditProfile(const QString &profileDirectory) {
  if (profileDirectory.isEmpty()) return false;
  const QString supplied = QDir::fromNativeSeparators(profileDirectory);
  const QFileInfo info(supplied);
  if (!info.isAbsolute() || supplied.startsWith(QStringLiteral("//"))) return false;
  for (const auto &part : supplied.split(QLatin1Char('/')))
    if (part == QStringLiteral("..") || part == QStringLiteral(".")) return false;
  const QString absolute = QDir::cleanPath(info.absoluteFilePath());
  if (QDir(absolute).isRoot()) return false;
#ifdef Q_OS_WIN
  // Refuse drive-relative paths, UNC/device namespaces, alternate streams,
  // mapped network drives, and every existing reparse ancestor.
  if (absolute.size() < 4 || absolute.at(1) != QLatin1Char(':') || absolute.at(2) != QLatin1Char('/') ||
      absolute.mid(2).contains(QLatin1Char(':'))) return false;
  const QString root = absolute.left(3);
  const UINT driveType = GetDriveTypeW(reinterpret_cast<LPCWSTR>(root.utf16()));
  if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE && driveType != DRIVE_RAMDISK) return false;
#endif
  if (!safeExistingAncestors(absolute)) return false;
  if (!QDir(absolute).exists() && !QDir().mkpath(absolute)) return false;
  return safeExistingAncestors(absolute);
}

NativeDiagnosticsCollector::NativeDiagnosticsCollector(QString runDirectory, QString sourceCommit, QString id, QObject *parent)
    : QObject(parent), state_(new State{std::move(runDirectory), std::move(sourceCommit), std::move(id), utcNow()}) {}
NativeDiagnosticsCollector::~NativeDiagnosticsCollector() { finalize(); delete state_; }

NativeDiagnosticsCollector *NativeDiagnosticsCollector::startIfRequested(const QString &profileDirectory,
                                                                           const QString &sourceCommit,
                                                                           QObject *parent) {
  if (qEnvironmentVariable("PRECISION_LAYOUT_AUDIT") != QStringLiteral("1")) return nullptr;
  if (!validSource(sourceCommit) || !hasValidatedAuditProfile(profileDirectory)) return nullptr;
  auto &dispatch = dispatcher();
  // Serialize starts without holding the counter lock across any Qt operation.
  std::lock_guard startupLock(dispatch.startupMutex);
  {
    std::lock_guard lock(dispatch.mutex);
    if (dispatch.active) return nullptr;
  }
  const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QString directory = QDir(profileDirectory).filePath(QStringLiteral("native-diagnostics-") + id);
  if (!createPrivateDirectory(directory) || !safeExistingAncestors(directory)) return nullptr;
  auto collector = std::unique_ptr<NativeDiagnosticsCollector>(new NativeDiagnosticsCollector(directory, sourceCommit, id, parent));
  // A failed setup cannot finalize and cannot relabel a previous run as current.
  if (!collector->writeSnapshot(false)) return nullptr;
  const QJsonObject expected{
      {QStringLiteral("schemaVersion"), 2}, {QStringLiteral("runId"), id},
      {QStringLiteral("sourceCommit"), sourceCommit}, {QStringLiteral("processId"), QCoreApplication::applicationPid()},
      {QStringLiteral("startedAtUtc"), collector->state_->startedAt},
      {QStringLiteral("receiptRelativePath"), QStringLiteral("native-diagnostics-") + id + QStringLiteral("/native-diagnostics.json")}};
  if (!atomicWrite(QDir(profileDirectory).filePath(QStringLiteral("native-diagnostics-run.json")), expected)) return nullptr;
  std::call_once(dispatch.installation, [&dispatch] {
    dispatch.previous.store(qInstallMessageHandler(&NativeDiagnosticsCollector::messageHandler), std::memory_order_release);
  });
  {
    std::lock_guard lock(dispatch.mutex);
    dispatch.active = collector->state_->counts;
    collector->state_->registered = true;
  }
  auto *timer = new QTimer(collector.get());
  timer->setInterval(1000);
  QObject::connect(timer, &QTimer::timeout, collector.get(), [value = collector.get()] { value->heartbeat(); });
  timer->start();
  return collector.release();
}
void NativeDiagnosticsCollector::watch(QQmlEngine *engine) {
  if (!engine || state_->finalized) return;
  const auto counts = state_->counts;
  connect(engine, &QQmlEngine::warnings, this, [counts](const QList<QQmlError> &warnings) {
    std::lock_guard lock(dispatcher().mutex);
    if (counts->accepting) increment(counts->qmlWarnings, static_cast<quint64>(warnings.size()), counts->overflow);
  }, Qt::DirectConnection);
}
bool NativeDiagnosticsCollector::isValid() const {
  std::lock_guard lock(dispatcher().mutex);
  return !state_->writeFailed && !state_->counts->overflow && !state_->counts->reentrant && state_->counts->qtFatals == 0;
}
QString NativeDiagnosticsCollector::receiptPath() const { return QDir(state_->runDirectory).filePath(QStringLiteral("native-diagnostics.json")); }
QString NativeDiagnosticsCollector::runId() const { return state_->id; }
#ifdef PRECISION_DIAGNOSTICS_TESTING
void NativeDiagnosticsCollector::seedWarningCountForTest(quint64 count) {
  std::lock_guard lock(dispatcher().mutex);
  state_->counts->qtWarnings = count > kMaximumCount ? kMaximumCount : count;
  state_->counts->overflow = count > kMaximumCount;
}
#endif

void NativeDiagnosticsCollector::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
  auto &dispatch = dispatcher();
  static thread_local bool entered = false;
  if (entered) {
    // A forwarding cycle and a genuinely nested diagnostic cannot be reliably
    // distinguished. Never certify ambiguous counts or count one event twice.
    std::lock_guard lock(dispatch.mutex);
    if (dispatch.active) dispatch.active->reentrant = true;
    return;
  }
  entered = true;
  struct ResetEntry { bool &value; ~ResetEntry() { value = false; } } reset{entered};
  {
    // No QObject, Qt synchronization, allocation, queueing or file I/O.
    std::lock_guard lock(dispatch.mutex);
    if (dispatch.active) {
      auto &counts = *dispatch.active;
      switch (type) {
        case QtWarningMsg: increment(counts.qtWarnings, 1, counts.overflow); break;
        case QtCriticalMsg: increment(counts.qtCriticals, 1, counts.overflow); break;
        case QtFatalMsg: increment(counts.qtFatals, 1, counts.overflow); break;
        default: break;
      }
    }
  }
  // Retain forwarding after deactivation and never install above a later handler.
  // A null prior handler means audit mode keeps only counts, not default raw logs.
  const auto previous = dispatch.previous.load(std::memory_order_acquire);
  if (previous && previous != &NativeDiagnosticsCollector::messageHandler) {
    previous(type, context, message);
  }
}
void NativeDiagnosticsCollector::heartbeat() { if (!state_->finalized) writeSnapshot(false); }
bool NativeDiagnosticsCollector::finalize() {
  if (QThread::currentThread() != thread()) return false;
  if (state_->finalized) return state_->sealed;
  if (!state_->registered) return false;
  {
    std::lock_guard lock(dispatcher().mutex);
    if (dispatcher().active == state_->counts) dispatcher().active.reset();
    state_->counts->accepting = false;
  }
  state_->finalized = true;
  state_->sealed = writeSnapshot(true);
  return state_->sealed;
}
bool NativeDiagnosticsCollector::writeSnapshot(bool final) {
  Counts counts;
  {
    std::lock_guard lock(dispatcher().mutex);
    counts = *state_->counts;
  }
  bool overflow = counts.overflow;
  increment(state_->sequence, 1, overflow);
  if (overflow) state_->writeFailed = true;
  const bool complete = final && !state_->writeFailed && !overflow && !counts.reentrant && counts.qtFatals == 0 && validSource(state_->sourceCommit);
  const QJsonObject record{
      {QStringLiteral("schemaVersion"), 2}, {QStringLiteral("runId"), state_->id},
      {QStringLiteral("startedAtUtc"), state_->startedAt}, {QStringLiteral("updatedAtUtc"), utcNow()},
      {QStringLiteral("sourceCommit"), state_->sourceCommit}, {QStringLiteral("processId"), QCoreApplication::applicationPid()},
      {QStringLiteral("qtWarnings"), static_cast<qint64>(counts.qtWarnings)},
      {QStringLiteral("qtCriticals"), static_cast<qint64>(counts.qtCriticals)},
      {QStringLiteral("qtFatals"), static_cast<qint64>(counts.qtFatals)},
      {QStringLiteral("qmlWarnings"), static_cast<qint64>(counts.qmlWarnings)},
      {QStringLiteral("heartbeat"), static_cast<qint64>(state_->sequence)},
      {QStringLiteral("countsComplete"), complete}, {QStringLiteral("sealed"), complete},
      {QStringLiteral("writeFailed"), state_->writeFailed}, {QStringLiteral("droppedEvents"), overflow || counts.reentrant},
      {QStringLiteral("reentrantEvents"), counts.reentrant}};
  if (!atomicWrite(receiptPath(), record)) { state_->writeFailed = true; return false; }
  return !final || complete;
}
} // namespace precision::diagnostics
