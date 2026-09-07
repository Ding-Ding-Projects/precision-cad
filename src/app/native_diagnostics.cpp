#include "native_diagnostics.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QQmlEngine>
#include <QSaveFile>
#include <QTimer>

#include <limits>

namespace precision::diagnostics {
namespace {
QMutex handlerMutex;
NativeDiagnosticsCollector *activeCollector = nullptr;
QtMessageHandler originalHandler = nullptr;
constexpr quint64 kMaximumCount = std::numeric_limits<quint64>::max() - 1;

QString utcNow() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }

bool increment(quint64 &value) {
  if (value >= kMaximumCount) return false;
  ++value;
  return true;
}
} // namespace

struct NativeDiagnosticsCollector::State {
  QMutex mutex;
  QString outputPath;
  QString sourceCommit;
  QString startedAt;
  quint64 qtWarnings = 0;
  quint64 qtCriticals = 0;
  quint64 qtFatals = 0;
  quint64 qmlWarnings = 0;
  quint64 heartbeat = 0;
  bool invalid = false;
  bool dropped = false;
};

bool hasValidatedAuditProfile(const QString &profileDirectory) {
  if (profileDirectory.isEmpty()) return false;
  const QFileInfo supplied(profileDirectory);
  if (!supplied.isAbsolute()) return false;
  const QString absolute = QDir::cleanPath(supplied.absoluteFilePath());
  if (absolute == QDir::rootPath() || absolute.contains(QStringLiteral("/../"))) return false;
  QDir dir(absolute);
  return dir.exists() || QDir().mkpath(absolute);
}

NativeDiagnosticsCollector::NativeDiagnosticsCollector(QString outputPath, QString sourceCommit, QObject *parent)
    : QObject(parent), state_(new State{.outputPath = std::move(outputPath), .sourceCommit = std::move(sourceCommit), .startedAt = utcNow()}) {}

NativeDiagnosticsCollector::~NativeDiagnosticsCollector() {
  QMutexLocker lock(&handlerMutex);
  if (activeCollector == this) {
    const QtMessageHandler current = qInstallMessageHandler(nullptr);
    qInstallMessageHandler(current == &NativeDiagnosticsCollector::messageHandler ? originalHandler : current);
    activeCollector = nullptr;
    originalHandler = nullptr;
  }
  delete state_;
}

NativeDiagnosticsCollector *NativeDiagnosticsCollector::startIfRequested(const QString &profileDirectory,
                                                                           const QString &sourceCommit,
                                                                           QObject *parent) {
  if (qEnvironmentVariable("PRECISION_LAYOUT_AUDIT") != QByteArrayLiteral("1")) return nullptr;
  if (!hasValidatedAuditProfile(profileDirectory)) return nullptr;
  auto *collector = new NativeDiagnosticsCollector(QDir(profileDirectory).filePath(QStringLiteral("native-diagnostics.json")), sourceCommit, parent);
  bool alreadyActive = false;
  {
    QMutexLocker lock(&handlerMutex);
    if (activeCollector != nullptr) {
      alreadyActive = true;
    } else {
      activeCollector = collector;
      originalHandler = qInstallMessageHandler(&NativeDiagnosticsCollector::messageHandler);
    }
  }
  if (alreadyActive) { delete collector; return nullptr; }
  collector->writeSnapshot();
  auto *timer = new QTimer(collector);
  timer->setInterval(1000);
  QObject::connect(timer, &QTimer::timeout, collector, [collector] { collector->heartbeat(); });
  timer->start();
  return collector;
}

void NativeDiagnosticsCollector::watch(QQmlEngine *engine) {
  if (!engine) return;
  connect(engine, &QQmlEngine::warnings, this, [this](const QList<QQmlError> &warnings) {
    recordQmlWarnings(warnings.size());
  }, Qt::DirectConnection);
}

bool NativeDiagnosticsCollector::isValid() const {
  QMutexLocker lock(&state_->mutex);
  return !state_->invalid && !state_->dropped;
}

void NativeDiagnosticsCollector::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
  Q_UNUSED(context)
  Q_UNUSED(message)
  QtMessageHandler previous = nullptr;
  {
    QMutexLocker lock(&handlerMutex);
    if (activeCollector) activeCollector->recordQtMessage(type);
    previous = originalHandler;
  }
  if (previous) previous(type, context, message);
}

void NativeDiagnosticsCollector::recordQtMessage(QtMsgType type) {
  {
    QMutexLocker lock(&state_->mutex);
    bool counted = true;
    switch (type) {
      case QtWarningMsg: counted = increment(state_->qtWarnings); break;
      case QtCriticalMsg: counted = increment(state_->qtCriticals); break;
      case QtFatalMsg: counted = increment(state_->qtFatals); break;
      default: break;
    }
    if (!counted) { state_->dropped = true; state_->invalid = true; }
  }
  scheduleSnapshot();
}

void NativeDiagnosticsCollector::recordQmlWarnings(qsizetype count) {
  {
    QMutexLocker lock(&state_->mutex);
    if (count < 0 || static_cast<quint64>(count) > kMaximumCount - state_->qmlWarnings) {
      state_->dropped = true;
      state_->invalid = true;
    } else {
      state_->qmlWarnings += static_cast<quint64>(count);
    }
  }
  writeSnapshot();
}

void NativeDiagnosticsCollector::heartbeat() {
  {
    QMutexLocker lock(&state_->mutex);
    if (!increment(state_->heartbeat)) { state_->dropped = true; state_->invalid = true; }
  }
  writeSnapshot();
}

void NativeDiagnosticsCollector::scheduleSnapshot() {
  QMetaObject::invokeMethod(this, [this] { writeSnapshot(); }, Qt::QueuedConnection);
}

bool NativeDiagnosticsCollector::writeSnapshot() {
  QJsonObject result;
  QMutexLocker lock(&state_->mutex);
    result.insert(QStringLiteral("schemaVersion"), 1);
    result.insert(QStringLiteral("startedAtUtc"), state_->startedAt);
    result.insert(QStringLiteral("updatedAtUtc"), utcNow());
    result.insert(QStringLiteral("sourceCommit"), state_->sourceCommit);
    result.insert(QStringLiteral("processId"), static_cast<qint64>(QCoreApplication::applicationPid()));
    result.insert(QStringLiteral("qtWarnings"), static_cast<qint64>(state_->qtWarnings));
    result.insert(QStringLiteral("qtCriticals"), static_cast<qint64>(state_->qtCriticals));
    result.insert(QStringLiteral("qtFatals"), static_cast<qint64>(state_->qtFatals));
    result.insert(QStringLiteral("qmlWarnings"), static_cast<qint64>(state_->qmlWarnings));
    result.insert(QStringLiteral("heartbeat"), static_cast<qint64>(state_->heartbeat));
    result.insert(QStringLiteral("countsComplete"), !state_->invalid && !state_->dropped);
    result.insert(QStringLiteral("droppedEvents"), state_->dropped);
  const QByteArray payload = QJsonDocument(result).toJson(QJsonDocument::Compact);
  QSaveFile output(state_->outputPath);
  if (!output.open(QIODevice::WriteOnly) || output.write(payload) != payload.size() || !output.commit()) {
    state_->invalid = true;
    return false;
  }
  return true;
}

} // namespace precision::diagnostics
