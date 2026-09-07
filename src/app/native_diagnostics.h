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
  // Owner-thread only, after the audited engines/workers stop. Destruction also
  // finalizes. Abnormal process termination never produces a sealed run.
  bool finalize();
  QString receiptPath() const;
  QString runId() const;
#ifdef PRECISION_DIAGNOSTICS_TESTING
  void seedWarningCountForTest(quint64 count);
#endif

private:
  NativeDiagnosticsCollector(QString runDirectory, QString sourceCommit, QString runId, QObject *parent);
  bool writeSnapshot(bool final);
  void heartbeat();
  static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message);

  struct State;
  State *state_;
};

bool hasValidatedAuditProfile(const QString &profileDirectory);

} // namespace precision::diagnostics
