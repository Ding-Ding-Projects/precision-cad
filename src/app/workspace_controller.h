#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QHash>
#include <memory>
#include "precision_document.h"

class WorkspaceController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList features READ features NOTIFY documentChanged)
  Q_PROPERTY(QVariantList meshVertices READ meshVertices NOTIFY meshChanged)
  Q_PROPERTY(QVariantList meshIndices READ meshIndices NOTIFY meshChanged)
  Q_PROPERTY(QString operationState READ operationState NOTIFY operationStateChanged)
  Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY operationStateChanged)
  Q_PROPERTY(QString volume READ volume NOTIFY measurementsChanged)
  Q_PROPERTY(QString bounds READ bounds NOTIFY measurementsChanged)
  Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
public:
  explicit WorkspaceController(QObject *parent = nullptr);
  ~WorkspaceController() override;
  QVariantList features() const; QVariantList meshVertices() const { return m_meshVertices; } QVariantList meshIndices() const { return m_meshIndices; }
  QString operationState() const { return m_state; } QString errorMessage() const { return m_error; } QString volume() const { return m_volume; } QString bounds() const { return m_bounds; } bool dirty() const { return m_dirty; }
  Q_INVOKABLE void addBox(double dx, double dy, double dz);
  Q_INVOKABLE void addCylinder(double radius, double height);
  Q_INVOKABLE void booleanOperation(const QString &operation, const QString &left, const QString &right);
  Q_INVOKABLE void suppressFeature(const QString &id, bool suppressed);
  Q_INVOKABLE void undo(); Q_INVOKABLE void redo(); Q_INVOKABLE void cancel(); Q_INVOKABLE void save(const QString &path); Q_INVOKABLE void open(const QString &path);
signals:
  void documentChanged(); void meshChanged(); void operationStateChanged(); void measurementsChanged(); void dirtyChanged(); void saveFinished(bool ok, const QString &message);
private:
  using Reply = QHash<QString, QJsonObject>;
  void applyTransaction(precision::core::Document candidate);
  void regenerate(precision::core::Document candidate);
  void runNext(); void fail(const QString &message); void commitCandidate();
  bool applyWorkerLimits(); void releaseWorkerLimits();
  QJsonObject requestFor(const precision::core::Feature &feature) const;
  precision::core::Document m_document;
  std::unique_ptr<precision::core::Document> m_candidate;
  QVector<precision::core::Feature> m_pending; int m_index = 0; Reply m_results;
  QProcess m_worker; QTimer m_timeout; quint64 m_generation = 0, m_loadedRevision = 0; QString m_path;
  QVariantList m_meshVertices, m_meshIndices; QString m_state = QStringLiteral("Ready"), m_error, m_volume = QStringLiteral("Unavailable"), m_bounds = QStringLiteral("Unavailable"); bool m_dirty = false;
  QByteArray m_activeRequest;
  void *m_job = nullptr;
};
