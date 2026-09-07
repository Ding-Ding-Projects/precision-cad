#pragma once

#include <QObject>
#include <QString>

class QQmlEngine;

namespace precision::diagnostics {

class NativeDiagnosticsCollector final : public QObject {
  Q_OBJECT
public:
  static NativeDiagnosticsCollector *startIfRequested(const QString &profileDirectory,
                                                       const QString &sourceCommit,
                                                       QObject *parent = nullptr);
  ~NativeDiagnosticsCollector() override;

  void watch(QQmlEngine *engine);
  bool isValid() const;

private:
  NativeDiagnosticsCollector(QString outputPath, QString sourceCommit, QObject *parent);
  bool writeSnapshot();
  void recordQtMessage(QtMsgType type);
  void recordQmlWarnings(qsizetype count);
  void heartbeat();
  void scheduleSnapshot();
  static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message);

  struct State;
  State *state_;
};

bool hasValidatedAuditProfile(const QString &profileDirectory);

} // namespace precision::diagnostics
