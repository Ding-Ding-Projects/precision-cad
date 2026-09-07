#include "workspace_controller.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace precision::core;
namespace { constexpr int kMaxWorkerReply = 8 * 1024 * 1024; constexpr int kWorkerTimeoutMs = 45000; int workerTimeoutMs() { bool ok=false; const int configured=qEnvironmentVariableIntValue("PRECISION_WORKER_TIMEOUT_MS",&ok); return ok ? std::clamp(configured,1,kWorkerTimeoutMs) : kWorkerTimeoutMs; } }

WorkspaceController::WorkspaceController(QObject *parent)
  : QObject(parent), m_document(DocumentRecord{kDocumentSchemaVersion, QUuid::createUuid().toString(QUuid::WithoutBraces), 0, QStringLiteral("mm"), {}}) {
  m_timeout.setSingleShot(true);
  connect(&m_timeout, &QTimer::timeout, this, [this] { cancel(); fail(tr("Geometry worker timed out.")); });
  connect(&m_worker, &QProcess::started, this, [this] {
    if (!applyWorkerLimits()) { m_worker.kill(); fail(tr("Geometry worker resource boundary could not be applied.")); return; }
    m_worker.write(m_activeRequest); m_worker.closeWriteChannel(); m_timeout.start(workerTimeoutMs());
  });
  connect(&m_worker, &QProcess::readyReadStandardOutput, this, [this] {
    if (m_worker.bytesAvailable() > kMaxWorkerReply) { cancel(); fail(tr("Geometry worker reply exceeded the configured limit.")); }
  });
  connect(&m_worker, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus status) {
    m_timeout.stop(); releaseWorkerLimits(); if (!m_candidate) return;
    const QByteArray output = m_worker.readAllStandardOutput(); const QByteArray errors = m_worker.readAllStandardError();
    if (output.size() > kMaxWorkerReply) { fail(tr("Geometry worker reply exceeded the configured limit.")); return; }
    if (status != QProcess::NormalExit || exitCode != 0) { fail(tr("Geometry worker stopped: %1").arg(QString::fromUtf8(errors.left(512)))); return; }
    QJsonParseError jsonError; const QJsonDocument json = QJsonDocument::fromJson(output, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !json.isObject()) { fail(tr("Geometry worker returned invalid JSON.")); return; }
    const QJsonObject reply = json.object(); const auto &record = m_candidate->record();
    if (!reply.value("ok").toBool() || reply.value("documentId").toString() != record.documentId || reply.value("revision").toVariant().toULongLong() != record.revision) { fail(reply.value("error").toObject().value("message").toString(tr("Geometry request failed."))); return; }
    m_results.insert(m_pending[m_index].id, reply.value("result").toObject()); ++m_index; runNext();
  });
}
WorkspaceController::~WorkspaceController() { cancel(); releaseWorkerLimits(); }
QVariantList WorkspaceController::features() const { QVariantList out; for (const Feature &f : m_document.record().features) out << QVariantMap{{"id", f.id}, {"label", f.label}, {"type", f.type}, {"suppressed", f.suppressed}}; return out; }
void WorkspaceController::addBox(double dx, double dy, double dz) { Feature f{QUuid::createUuid().toString(QUuid::WithoutBraces), "box", tr("Box"), {}, QJsonObject{{"dx", dx},{"dy",dy},{"dz",dz}}, false}; Document candidate=m_document; const Result result=candidate.addFeature(f,m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::addCylinder(double radius, double height) { Feature f{QUuid::createUuid().toString(QUuid::WithoutBraces), "cylinder", tr("Cylinder"), {}, QJsonObject{{"radius", radius},{"height",height}}, false}; Document candidate=m_document; const Result result=candidate.addFeature(f,m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::booleanOperation(const QString &operation, const QString &left, const QString &right) { if (left.isEmpty() || right.isEmpty() || left == right) { fail(tr("Choose two different bodies.")); return; } Feature f{QUuid::createUuid().toString(QUuid::WithoutBraces), operation, operation.left(1).toUpper()+operation.mid(1), {left,right}, {}, false}; Document candidate=m_document; const Result result=candidate.addFeature(f,m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::updateDimensions(const QString &id, double first, double second, double third) { Document candidate=m_document; for(Feature f : m_document.record().features) if(f.id==id) { if(f.type=="box") f.parameters={{"dx",first},{"dy",second},{"dz",third}}; else if(f.type=="cylinder") f.parameters={{"radius",first},{"height",second}}; else { fail(tr("This feature has no editable dimensions in the current workspace.")); return; } const Result r=candidate.updateFeature(f,m_document.record().revision); if(!r.ok) fail(r.error); else applyTransaction(std::move(candidate)); return; } fail(tr("Selected feature no longer exists.")); }
void WorkspaceController::suppressFeature(const QString &id, bool value) { Document candidate=m_document; const Result result=candidate.suppressFeature(id,value,m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::undo() { Document candidate=m_document; const Result result=candidate.undo(m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::redo() { Document candidate=m_document; const Result result=candidate.redo(m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::applyTransaction(Document candidate) { if (m_candidate) { fail(tr("A geometry operation is already running.")); return; } regenerate(std::move(candidate)); }
void WorkspaceController::regenerate(Document candidate) { m_candidate=std::make_unique<Document>(std::move(candidate)); m_pending.clear(); m_results.clear(); m_index=0; for (const Feature &f:m_candidate->record().features) if (!f.suppressed) m_pending.append(f); m_state=tr("Regenerating %1 feature(s)").arg(m_pending.size()); m_error.clear(); emit operationStateChanged(); if (m_pending.isEmpty()) { commitCandidate(); return; } runNext(); }
QJsonObject WorkspaceController::requestFor(const Feature &f) const { QJsonObject params=f.parameters; if (f.type=="union"||f.type=="cut"||f.type=="intersection") { params.insert("leftBrep",m_results.value(f.inputRefs.value(0)).value("brep")); params.insert("rightBrep",m_results.value(f.inputRefs.value(1)).value("brep")); } else if ((f.type=="translate"||f.type=="rotate"||f.type=="fillet") && !f.inputRefs.isEmpty()) params.insert("brep",m_results.value(f.inputRefs.first()).value("brep")); return {{"protocolVersion",1},{"operationId",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"documentId",m_candidate->record().documentId},{"revision",static_cast<qint64>(m_candidate->record().revision)},{"operation",f.type},{"parameters",params}}; }
void WorkspaceController::runNext() { if (!m_candidate) return; if (m_index>=m_pending.size()) { commitCandidate(); return; } const QJsonObject request=requestFor(m_pending[m_index]); const QJsonObject parameters=request.value("parameters").toObject(); if ((parameters.contains("leftBrep") && parameters.value("leftBrep").toString().isEmpty()) || (parameters.contains("rightBrep") && parameters.value("rightBrep").toString().isEmpty())) { fail(tr("A required feature result is unavailable or ambiguous.")); return; } QString worker=qEnvironmentVariable("PRECISION_GEOMETRY_WORKER"); if(worker.isEmpty()) worker=QCoreApplication::applicationDirPath()+"/precision_geometry_worker.exe"; QProcessEnvironment environment; const auto inherited=QProcessEnvironment::systemEnvironment(); for(const QString &key : {QStringLiteral("PATH"),QStringLiteral("SystemRoot"),QStringLiteral("TEMP"),QStringLiteral("TMP"),QStringLiteral("QT_PLUGIN_PATH"),QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH")}) if(inherited.contains(key)) environment.insert(key,inherited.value(key)); m_worker.setProcessEnvironment(environment); m_activeRequest=QJsonDocument(request).toJson(QJsonDocument::Compact); m_worker.start(worker); }
bool WorkspaceController::applyWorkerLimits() {
#ifdef Q_OS_WIN
  releaseWorkerLimits(); HANDLE job=CreateJobObjectW(nullptr,nullptr); if(!job) return false;
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{}; info.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_ACTIVE_PROCESS; info.ProcessMemoryLimit=512ull*1024ull*1024ull; info.BasicLimitInformation.ActiveProcessLimit=1;
  if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&info,sizeof(info))){CloseHandle(job);return false;} HANDLE process=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE,FALSE,static_cast<DWORD>(m_worker.processId())); if(!process){CloseHandle(job);return false;} const BOOL assigned=AssignProcessToJobObject(job,process); CloseHandle(process); if(!assigned){CloseHandle(job);return false;} m_job=job; return true;
#else
  return false;
#endif
}
void WorkspaceController::releaseWorkerLimits() {
#ifdef Q_OS_WIN
  if(m_job) CloseHandle(static_cast<HANDLE>(m_job));
#endif
  m_job=nullptr;
}
void WorkspaceController::commitCandidate() { const auto &record=m_candidate->record(); if(!m_pending.isEmpty()) { const QJsonObject result=m_results.value(m_pending.last().id); const QJsonObject mesh=result.value("mesh").toObject(); m_meshVertices=mesh.value("vertices").toArray().toVariantList(); m_meshIndices=mesh.value("indices").toArray().toVariantList(); m_volume=QString::number(result.value("volume").toDouble(),'g',12)+tr(" mm³"); const QJsonArray b=result.value("bounds").toArray(); if(b.size()==6) m_bounds=QStringLiteral("[%1, %2, %3] to [%4, %5, %6] mm").arg(b[0].toDouble()).arg(b[1].toDouble()).arg(b[2].toDouble()).arg(b[3].toDouble()).arg(b[4].toDouble()).arg(b[5].toDouble()); }
  m_document=std::move(*m_candidate); m_candidate.reset(); m_state=tr("Ready"); m_dirty=true; emit documentChanged(); emit meshChanged(); emit measurementsChanged(); emit operationStateChanged(); emit dirtyChanged(); }
void WorkspaceController::fail(const QString &message) { m_candidate.reset(); m_pending.clear(); m_results.clear(); m_state=tr("Failed"); m_error=message; emit operationStateChanged(); }
void WorkspaceController::cancel() { if(m_worker.state()!=QProcess::NotRunning) { m_worker.kill(); m_worker.waitForFinished(1000); } releaseWorkerLimits(); m_timeout.stop(); if(m_candidate) { m_candidate.reset(); m_state=tr("Cancelled"); emit operationStateChanged(); } }
void WorkspaceController::save(const QString &path) { if(path.isEmpty()) return; const Result r=DocumentStorage::save(path,m_document.record(),m_path.isEmpty()?std::nullopt:std::optional<quint64>(m_loadedRevision)); if(!r.ok){ fail(r.error); emit saveFinished(false,r.error); return; } m_path=path; m_loadedRevision=m_document.record().revision; m_dirty=false; emit dirtyChanged(); emit saveFinished(true,tr("Saved")); }
void WorkspaceController::open(const QString &path) { if(path.isEmpty()||m_candidate) return; DocumentRecord record; const Result r=DocumentStorage::recover(path,&record); if(!r.ok){ fail(r.error); return; } try { m_document=Document(record); } catch(const std::exception &e) { fail(QString::fromUtf8(e.what())); return; } m_path=path; m_loadedRevision=record.revision; m_dirty=false; m_meshVertices.clear();m_meshIndices.clear();m_volume=tr("Unavailable");m_bounds=tr("Unavailable");emit documentChanged();emit meshChanged();emit measurementsChanged();emit dirtyChanged(); regenerate(m_document); }
void WorkspaceController::newDocument() { if(m_candidate) return; m_document=Document(DocumentRecord{kDocumentSchemaVersion,QUuid::createUuid().toString(QUuid::WithoutBraces),0,QStringLiteral("mm"),{}}); m_path.clear();m_loadedRevision=0;m_dirty=false;m_meshVertices.clear();m_meshIndices.clear();m_volume=tr("Unavailable");m_bounds=tr("Unavailable");emit documentChanged();emit meshChanged();emit measurementsChanged();emit dirtyChanged(); }
