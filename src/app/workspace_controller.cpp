#include "workspace_controller.h"
#include "model_evaluator.h"
#include "guided_sketch.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <QDir>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace precision::core;
namespace { constexpr int kMaxWorkerReply = 8 * 1024 * 1024; constexpr int kWorkerTimeoutMs = 45000; constexpr qint64 kMaxResultCacheBytes = 64 * 1024 * 1024;
qint64 resultCacheLimit() { bool ok=false; const qint64 configured=qEnvironmentVariableIntValue("PRECISION_WORKER_CACHE_BYTES",&ok); return ok ? std::clamp<qint64>(configured,1,kMaxResultCacheBytes) : kMaxResultCacheBytes; }
 int workerTimeoutMs() { bool ok=false; const int configured=qEnvironmentVariableIntValue("PRECISION_WORKER_TIMEOUT_MS",&ok); return ok ? std::clamp(configured,1,kWorkerTimeoutMs) : kWorkerTimeoutMs; } }

WorkspaceController::WorkspaceController(QObject *parent)
  : QObject(parent), m_document(DocumentRecord{kDocumentSchemaVersion, QUuid::createUuid().toString(QUuid::WithoutBraces), 0, QStringLiteral("mm"), {}}) {
  m_timeout.setSingleShot(true);
  connect(&m_timeout, &QTimer::timeout, this, [this] { fail(tr("Geometry worker timed out.")); });
}
WorkspaceController::~WorkspaceController() { cancel(); releaseWorkerLimits(); }
void WorkspaceController::addSketch(const QString &plane,double width,double height,double radius,double u,double v) {
  if(busy()) return;
  const QString id=QUuid::createUuid().toString(QUuid::WithoutBraces);
  const auto model=rectangleHoleModel(id,plane,width,height,radius,u,v);
  if(model.isEmpty()) { fail(tr("Sketch dimensions and datum must be valid finite values.")); return; }
  Feature feature{id,"sketch",tr("Sketch"),{},{{"model",model}},false};
  Document candidate=m_document; const auto result=candidate.addFeature(feature,m_document.record().revision);
  if(!result.ok) fail(result.error); else applyTransaction(std::move(candidate));
}
QVariantMap WorkspaceController::editableSketch(const QString &id) const {
  for(const auto &feature:m_document.record().features) if(feature.id==id && feature.type=="sketch") return rectangleHoleDimensions(feature.parameters.value("model").toObject());
  return {{"editable",false}};
}
QVariantMap WorkspaceController::sketchDetails(const QString &id) const {
  QString source=id;
  for(const auto &feature:m_document.record().features) if(feature.id==id && feature.type=="pad") source=feature.inputRefs.value(0);
  const auto result=m_committedResults.value(source);
  if(result.value("kind")!=QJsonValue("sketch")) return {{"available",false},{"regions",QVariantList{}},{"preview",QVariantMap{{"segments",QVariantList{}}}}};
  const auto solve=result.value("solve").toObject();
  return {{"available",true},{"kind","sketch"},{"sourceFeatureId",source},{"status",solve.value("status").toString()},{"dof",solve.value("dof").toInt()},
    {"conflicts",solve.value("conflicts").toArray().toVariantList()},{"regions",result.value("profiles").toObject().value("regions").toArray().toVariantList()},
    {"preview",result.value("preview").toObject().toVariantMap()}};
}
void WorkspaceController::updateSketch(const QString &id,const QString &plane,double width,double height,double radius,double u,double v) {
  if(busy()) return;
  if(!editableSketch(id).value("editable").toBool()) { fail(tr("This sketch is not a guided rectangle and circular hole.")); return; }
  Document candidate=m_document;
  for(auto feature:m_document.record().features) if(feature.id==id) {
    const auto model=rectangleHoleModel(feature.parameters.value("model").toObject().value("id").toString(),plane,width,height,radius,u,v);
    if(model.isEmpty()) { fail(tr("Sketch dimensions and datum must be valid finite values.")); return; }
    feature.parameters={{"model",model}}; const auto result=candidate.updateFeature(feature,m_document.record().revision);
    if(!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); return;
  }
}
void WorkspaceController::addPad(const QString &sketchId,const QString &regionId,double length) {
  if(busy()) return;
  if(m_committedResults.value(sketchId).value("kind")!=QJsonValue("sketch")) { fail(tr("Select a solved sketch before adding a pad.")); return; }
  Feature feature{QUuid::createUuid().toString(QUuid::WithoutBraces),"pad",tr("Pad"),{sketchId},{{"regionId",regionId},{"length",length}},false};
  Document candidate=m_document; const auto result=candidate.addFeature(feature,m_document.record().revision);
  if(!result.ok) fail(result.error); else applyTransaction(std::move(candidate));
}
QVariantList WorkspaceController::features() const { QVariantList out; for (const Feature &f : m_document.record().features) out << QVariantMap{{"id", f.id}, {"label", f.label}, {"type", f.type}, {"suppressed", f.suppressed}}; return out; }
void WorkspaceController::addBox(double dx, double dy, double dz) { if(m_candidate) { m_error=tr("A geometry operation is already running."); emit operationStateChanged(); return; } Feature f{QUuid::createUuid().toString(QUuid::WithoutBraces), "box", tr("Box"), {}, QJsonObject{{"dx", dx},{"dy",dy},{"dz",dz}}, false}; Document candidate=m_document; const Result result=candidate.addFeature(f,m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::addCylinder(double radius, double height) { if(m_candidate) { m_error=tr("A geometry operation is already running."); emit operationStateChanged(); return; } Feature f{QUuid::createUuid().toString(QUuid::WithoutBraces), "cylinder", tr("Cylinder"), {}, QJsonObject{{"radius", radius},{"height",height}}, false}; Document candidate=m_document; const Result result=candidate.addFeature(f,m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::booleanOperation(const QString &operation, const QString &left, const QString &right) { if(m_candidate) { m_error=tr("A geometry operation is already running."); emit operationStateChanged(); return; } if (left.isEmpty() || right.isEmpty() || left == right) { fail(tr("Choose two different bodies.")); return; } Feature f{QUuid::createUuid().toString(QUuid::WithoutBraces), operation, operation.left(1).toUpper()+operation.mid(1), {left,right}, {}, false}; Document candidate=m_document; const Result result=candidate.addFeature(f,m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
QVariantMap WorkspaceController::editableDimensions(const QString &id) const { for(const Feature &f:m_document.record().features) if(f.id==id) { auto finitePositive=[](double value) { return std::isfinite(value) && value>0; }; if(f.type=="pad") return {{"editable",true},{"type","pad"},{"first",f.parameters.value("length").toDouble()}}; if(f.type=="box") { const double dx=f.parameters.value("dx").toDouble(), dy=f.parameters.value("dy").toDouble(), dz=f.parameters.value("dz").toDouble(); if(finitePositive(dx)&&finitePositive(dy)&&finitePositive(dz)) return {{"editable",true},{"type","box"},{"first",dx},{"second",dy},{"third",dz}}; } else if(f.type=="cylinder") { const double radius=f.parameters.value("radius").toDouble(), height=f.parameters.value("height").toDouble(); if(finitePositive(radius)&&finitePositive(height)) return {{"editable",true},{"type","cylinder"},{"first",radius},{"second",height}}; } return {{"editable",false}}; } return {{"editable",false}}; }
void WorkspaceController::updateDimensions(const QString &id, double first, double second, double third) { if(m_candidate) { m_error=tr("A geometry operation is already running."); emit operationStateChanged(); return; } Document candidate=m_document; for(Feature f : m_document.record().features) if(f.id==id) { const auto valid=[](double value) { return std::isfinite(value)&&value>0; }; if(f.type=="pad") { if(!valid(first)) { fail(tr("Pad length must be finite and positive.")); return; } f.parameters.insert("length",first); } else if(f.type=="box") { if(!valid(first)||!valid(second)||!valid(third)) { fail(tr("Dimensions must be finite positive values.")); return; } f.parameters={{"dx",first},{"dy",second},{"dz",third}}; } else if(f.type=="cylinder") { if(!valid(first)||!valid(second)) { fail(tr("Dimensions must be finite positive values.")); return; } f.parameters={{"radius",first},{"height",second}}; } else { fail(tr("This feature has no editable dimensions in the current workspace.")); return; } const Result r=candidate.updateFeature(f,m_document.record().revision); if(!r.ok) fail(r.error); else applyTransaction(std::move(candidate)); return; } fail(tr("Selected feature no longer exists.")); }
void WorkspaceController::suppressFeature(const QString &id, bool value) { if(m_candidate) { m_error=tr("A geometry operation is already running."); emit operationStateChanged(); return; } Document candidate=m_document; const Result result=candidate.suppressFeature(id,value,m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::undo() { if(m_candidate) { m_error=tr("A geometry operation is already running."); emit operationStateChanged(); return; } Document candidate=m_document; const Result result=candidate.undo(m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::redo() { if(m_candidate) { m_error=tr("A geometry operation is already running."); emit operationStateChanged(); return; } Document candidate=m_document; const Result result=candidate.redo(m_document.record().revision); if (!result.ok) fail(result.error); else applyTransaction(std::move(candidate)); }
void WorkspaceController::applyTransaction(Document candidate) { if (m_candidate) { m_error=tr("A geometry operation is already running."); emit operationStateChanged(); return; } regenerate(std::move(candidate)); }
void WorkspaceController::regenerate(Document candidate) { ++m_generation; m_baseId=m_document.record().documentId; m_baseRevision=m_document.record().revision; m_candidate=std::make_unique<Document>(std::move(candidate)); m_pending.clear(); m_results.clear(); m_resultBytes=0; m_resultLimit=resultCacheLimit(); m_index=0;
  const auto evaluation=ModelEvaluator::evaluate(m_candidate->record());
  if(!evaluation.result.ok) { fail(evaluation.result.error); return; }
  for (const auto &entry : evaluation.ordered) if (entry.state==EvaluationState::Ready) m_pending.append(entry.feature);
  m_state=tr("Regenerating %1 feature(s)").arg(m_pending.size()); m_error.clear(); emit operationStateChanged(); if (m_pending.isEmpty()) { commitCandidate(); return; } runNext(); }
QJsonObject WorkspaceController::requestFor(const Feature &f) const { QJsonObject params=f.parameters; if (f.type=="union"||f.type=="cut"||f.type=="intersection") { params.insert("leftBrep",m_results.value(f.inputRefs.value(0)).value("brep")); params.insert("rightBrep",m_results.value(f.inputRefs.value(1)).value("brep")); } else if ((f.type=="translate"||f.type=="rotate"||f.type=="fillet"||f.type=="validate"||f.type=="tessellate") && !f.inputRefs.isEmpty()) params.insert("brep",m_results.value(f.inputRefs.first()).value("brep")); if(f.type=="sketch") params.insert("producerFeatureId",f.id); if(f.type=="pad") { params.insert("sketchId",f.inputRefs.value(0)); params.insert("sketch",m_results.value(f.inputRefs.value(0))); } return {{"protocolVersion",(f.type=="sketch"||f.type=="pad")?2:1},{"operationId",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"documentId",m_candidate->record().documentId},{"revision",static_cast<qint64>(m_candidate->record().revision)},{"operation",f.type},{"parameters",params}}; }
void WorkspaceController::runNext() { if (!m_candidate) return; if (m_index>=m_pending.size()) { commitCandidate(); return; } const QJsonObject request=requestFor(m_pending[m_index]); const QJsonObject parameters=request.value("parameters").toObject(); if ((parameters.contains("leftBrep") && parameters.value("leftBrep").toString().isEmpty()) || (parameters.contains("rightBrep") && parameters.value("rightBrep").toString().isEmpty())) { fail(tr("A required feature result is unavailable or ambiguous.")); return; } QString worker=qEnvironmentVariable("PRECISION_GEOMETRY_WORKER"); if(worker.isEmpty()) worker=QCoreApplication::applicationDirPath()+"/precision_geometry_worker.exe"; QProcessEnvironment environment; const auto inherited=QProcessEnvironment::systemEnvironment(); for(const QString &key : {QStringLiteral("PATH"),QStringLiteral("SystemRoot"),QStringLiteral("TEMP"),QStringLiteral("TMP"),QStringLiteral("QT_PLUGIN_PATH"),QStringLiteral("QT_QPA_PLATFORM_PLUGIN_PATH")}) if(inherited.contains(key)) environment.insert(key,inherited.value(key)); stopWorker(); m_output.clear(); m_errors.clear(); m_operationId=request.value("operationId").toString();
  m_worker=std::make_unique<QProcess>(); const quint64 epoch=m_generation; QProcess *process=m_worker.get();
  auto current=[this,epoch,process] { return m_candidate && epoch==m_generation && m_worker.get()==process; };
  connect(process,&QProcess::started,this,[this,current] { if(!current()) return; if(!applyWorkerLimits()) { fail(tr("Geometry worker resource boundary could not be applied.")); return; } m_worker->write(m_activeRequest); m_worker->closeWriteChannel(); });
  connect(process,&QProcess::errorOccurred,this,[this,current](QProcess::ProcessError error) { if(current() && error==QProcess::FailedToStart) fail(tr("Geometry worker could not start.")); });
  connect(process,&QProcess::readyReadStandardOutput,this,[this,current] { if(current()) drainWorker(false); });
  connect(process,&QProcess::readyReadStandardError,this,[this,current] { if(current()) drainWorker(true); });
  connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,current](int code,QProcess::ExitStatus status) {
    if(!current()) return; drainWorker(false); if(!current()) return; drainWorker(true); if(!current()) return;
    m_timeout.stop(); releaseWorkerLimits();
    if((code!=0 && code!=2) || status!=QProcess::NormalExit) { fail(tr("Geometry worker stopped without a valid result.")); return; }
    QJsonParseError error; const auto json=QJsonDocument::fromJson(m_output,&error); const auto reply=json.object();
    const auto &record=m_candidate->record();
    const bool matched=error.error==QJsonParseError::NoError && json.isObject() && reply.size()==6 && reply.value("protocolVersion")==QJsonValue((m_pending[m_index].type=="sketch"||m_pending[m_index].type=="pad")?2:1) && reply.value("operationId")==QJsonValue(m_operationId) && reply.value("documentId")==QJsonValue(record.documentId) && reply.value("revision")==QJsonValue(static_cast<qint64>(record.revision));
    if(matched && reply.value("ok")==QJsonValue(false)) {
      const auto detail=reply.value("error").toObject();
      if(detail.size()==2 && detail.value("code").isString() && detail.value("message").isString() && detail.value("message").toString().size()<=2048) { fail(detail.value("message").toString()); return; }
    }
    if(code!=0 || reply.size()!=6 || error.error!=QJsonParseError::NoError || !json.isObject() || reply.value("protocolVersion")!=QJsonValue((m_pending[m_index].type=="sketch"||m_pending[m_index].type=="pad")?2:1) || reply.value("operationId")!=QJsonValue(m_operationId) || reply.value("documentId")!=QJsonValue(record.documentId) || reply.value("revision")!=QJsonValue(static_cast<qint64>(record.revision)) || reply.value("ok")!=QJsonValue(true) || !validResult(reply.value("result").toObject())) { fail(tr("Geometry worker returned an invalid or mismatched result.")); return; }
    const QJsonObject result=reply.value("result").toObject();
    const qint64 resultBytes=QJsonDocument(result).toJson(QJsonDocument::Compact).size();
    if(resultBytes>m_resultLimit-m_resultBytes) { fail(tr("Geometry result cache exceeded the configured limit.")); return; }
    m_resultBytes+=resultBytes; m_results.insert(m_pending[m_index].id,result); ++m_index;
    QTimer::singleShot(0,this,[this,epoch=m_generation] { if(m_candidate && epoch==m_generation) runNext(); });
  });
  m_worker->setProcessEnvironment(environment); m_activeRequest=QJsonDocument(request).toJson(QJsonDocument::Compact); m_timeout.start(workerTimeoutMs()); m_worker->start(worker); }
bool WorkspaceController::applyWorkerLimits() {
#ifdef Q_OS_WIN
  releaseWorkerLimits(); HANDLE job=CreateJobObjectW(nullptr,nullptr); if(!job) return false;
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{}; info.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_PROCESS_MEMORY|JOB_OBJECT_LIMIT_JOB_MEMORY|JOB_OBJECT_LIMIT_ACTIVE_PROCESS; info.ProcessMemoryLimit=512ull*1024ull*1024ull; info.JobMemoryLimit=info.ProcessMemoryLimit; info.BasicLimitInformation.ActiveProcessLimit=1;
  if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&info,sizeof(info))){CloseHandle(job);return false;} HANDLE process=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE,FALSE,static_cast<DWORD>(m_worker->processId())); if(!process){CloseHandle(job);return false;} const BOOL assigned=AssignProcessToJobObject(job,process); CloseHandle(process); if(!assigned){CloseHandle(job);return false;} m_job=job; return true;
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
bool WorkspaceController::validResult(const QJsonObject &result) const {
  if(m_candidate && m_index<m_pending.size() && m_pending[m_index].type=="sketch") {
    if(result.size()!=8 || result.value("kind")!=QJsonValue("sketch") || result.value("producerFeatureId")!=QJsonValue(m_pending[m_index].id) || result.value("documentId")!=QJsonValue(m_candidate->record().documentId) || result.value("revision")!=QJsonValue(static_cast<qint64>(m_candidate->record().revision)) || result.value("model")!=m_pending[m_index].parameters.value("model")) return false;
    const auto solve=result.value("solve").toObject(); const QString status=solve.value("status").toString();
    const double dof=solve.value("dof").toDouble(-1);
    if(solve.size()!=3 || (status!="solved" && status!="underConstrained") || !std::isfinite(dof) || dof<0 || dof>8192 || std::floor(dof)!=dof || (status=="solved")!=(dof==0) || !solve.value("conflicts").isArray() || !solve.value("conflicts").toArray().isEmpty()) return false;
    const auto profiles=result.value("profiles").toObject(), preview=result.value("preview").toObject();
    const auto loops=profiles.value("loops").toArray(), regions=profiles.value("regions").toArray();
    if(profiles.size()!=2 || loops.isEmpty() || loops.size()>4096 || regions.isEmpty() || regions.size()>4096 || preview.size()!=1 || !preview.value("segments").isArray()) return false;
    QSet<QString> loopIds, regionIds, sourceIds; QJsonArray flattened;
    for(const auto &value:loops) {
      const auto loop=value.toObject(); const QString id=loop.value("stableId").toString(); const auto segments=loop.value("segments").toArray();
      if(loop.size()!=3 || id.isEmpty() || id.size()>1024 || loopIds.contains(id) || !loop.value("clockwise").isBool() || segments.isEmpty() || segments.size()>4096) return false;
      loopIds.insert(id);
      for(const auto &entry:segments) {
        const auto segment=entry.toObject(); const QString kind=segment.value("kind").toString(), source=segment.value("sourceEntityId").toString();
        if(segment.size()!=11 || (kind!="line" && kind!="circle") || source.isEmpty() || sourceIds.contains(source) || !segment.value("startPointId").isString() || !segment.value("endPointId").isString()) return false;
        sourceIds.insert(source);
        for(const char *key:{"startU","startV","endU","endV","centerU","centerV","radius"}) { auto n=segment.value(key); if(!n.isDouble() || !std::isfinite(n.toDouble()) || std::abs(n.toDouble())>1e9) return false; }
        if(kind=="circle" && segment.value("radius").toDouble()<=0) return false;
        flattened.append(entry);
      }
    }
    for(const auto &value:regions) {
      const auto region=value.toObject(); const QString id=region.value("stableId").toString(), outer=region.value("outerLoopId").toString();
      if(region.size()!=3 || id.isEmpty() || id.size()>1024 || regionIds.contains(id) || !loopIds.contains(outer) || !region.value("holeLoopIds").isArray()) return false;
      regionIds.insert(id); QSet<QString> holes;
      for(const auto &hole:region.value("holeLoopIds").toArray()) { if(!hole.isString() || !loopIds.contains(hole.toString()) || hole.toString()==outer || holes.contains(hole.toString())) return false; holes.insert(hole.toString()); }
    }
    return flattened==preview.value("segments").toArray();
  }
  if(result.size()!=5) return false;
  if(result.value("valid")!=QJsonValue(true) || !result.value("brep").isString() || result.value("brep").toString().trimmed().isEmpty()) return false;
  auto finite=[](QJsonValue value) { return value.isDouble() && std::isfinite(value.toDouble()); };
  if(!finite(result.value("volume")) || result.value("volume").toDouble()<0) return false;
  const auto bounds=result.value("bounds").toArray(); if(bounds.size()!=6) return false;
  for(auto value:bounds) if(!finite(value) || std::abs(value.toDouble())>1.0e9) return false;
  for(int i=0;i<3;++i) if(bounds[i].toDouble()>bounds[i+3].toDouble()) return false;
  const auto mesh=result.value("mesh").toObject(); const auto vertices=mesh.value("vertices").toArray(), indices=mesh.value("indices").toArray(), normals=mesh.value("normals").toArray();
  if(mesh.size()!=6) return false;
  if(vertices.isEmpty() || vertices.size()%3 || vertices.size()>180000 || indices.isEmpty() || indices.size()%3 || indices.size()>300000 || normals.size()!=vertices.size()) return false;
  for(auto value:vertices) if(!finite(value) || std::abs(value.toDouble())>1.0e9) return false;
  for(auto value:normals) if(!finite(value)) return false;
  for(auto value:indices) if(!finite(value) || value.toDouble()<0 || std::floor(value.toDouble())!=value.toDouble() || value.toDouble()>=vertices.size()/3) return false;
  for(const QString &key:{QStringLiteral("absoluteDeflection"),QStringLiteral("targetRelativeDeflection"),QStringLiteral("effectiveRelativeDeflection")}) if(!finite(mesh.value(key)) || mesh.value(key).toDouble()<=0) return false;
  return true;
}
void WorkspaceController::drainWorker(bool errors) {
  if(!m_worker) return;
  m_worker->setReadChannel(errors?QProcess::StandardError:QProcess::StandardOutput);
  QByteArray &buffer=errors?m_errors:m_output;
  while(m_worker->bytesAvailable()>0) {
    const QByteArray chunk=m_worker->read(std::min<qint64>(65536,kMaxWorkerReply-buffer.size()+1)); buffer.append(chunk);
    if(buffer.size()>kMaxWorkerReply) { fail(tr("Geometry worker output exceeded the configured limit.")); return; }
  }
}
void WorkspaceController::stopWorker() {
  m_timeout.stop(); if(!m_worker) { releaseWorkerLimits(); return; }
  QProcess *old=m_worker.release(); old->disconnect(this); if(old->state()!=QProcess::NotRunning) { old->kill(); old->waitForFinished(1000); }
  releaseWorkerLimits(); old->deleteLater();
}
void WorkspaceController::selectBody(const QString &id) {
  if(!id.isEmpty() && !m_committedResults.contains(id)) return;
  m_selectedBody=id; updateScene();
}
QVector<QString> WorkspaceController::terminalBodyIds() const {
  QSet<QString> consumed;
  for(const Feature &feature:m_committedFeatures) if(m_committedResults.contains(feature.id)) for(const QString &input:feature.inputRefs) consumed.insert(input);
  QVector<QString> ids;
  for(const Feature &feature:m_committedFeatures) if(m_committedResults.contains(feature.id) && !consumed.contains(feature.id)) ids.append(feature.id);
  return ids;
}
void WorkspaceController::updateScene() {
  const QVector<QString> terminals=terminalBodyIds(); QVector<QString> visible;
  if(m_selectedBody.isEmpty() || terminals.contains(m_selectedBody)) visible=terminals; else visible={m_selectedBody};
  m_meshParts.clear();
  for(const QString &id:visible) { if(m_committedResults.value(id).value("kind")==QJsonValue("sketch")) continue; const auto mesh=m_committedResults.value(id).value("mesh").toObject(); m_meshParts << QVariantMap{{"bodyId",id},{"vertices",mesh.value("vertices").toArray().toVariantList()},{"indices",mesh.value("indices").toArray().toVariantList()},{"normals",mesh.value("normals").toArray().toVariantList()}}; }
  const auto result=m_committedResults.value(m_selectedBody); const auto mesh=result.value("mesh").toObject();
  m_meshVertices=mesh.value("vertices").toArray().toVariantList(); m_meshIndices=mesh.value("indices").toArray().toVariantList(); m_meshNormals=mesh.value("normals").toArray().toVariantList();
  m_volume=(result.isEmpty()||result.value("kind")==QJsonValue("sketch"))?tr("Unavailable"):QString::number(result.value("volume").toDouble(),'g',12)+tr(" mm\u00b3");
  const auto b=result.value("bounds").toArray(); m_bounds=b.size()!=6?tr("Unavailable"):QStringLiteral("[%1, %2, %3] to [%4, %5, %6] mm").arg(b[0].toDouble()).arg(b[1].toDouble()).arg(b[2].toDouble()).arg(b[3].toDouble()).arg(b[4].toDouble()).arg(b[5].toDouble()); emit meshChanged(); emit measurementsChanged();
}
void WorkspaceController::commitCandidate() {
  if(!m_candidate || m_document.record().documentId!=m_baseId || m_document.record().revision!=m_baseRevision) { fail(tr("Document changed while geometry was running.")); return; }
  QSet<QString> previousIds; for(const Feature &feature:m_document.record().features) previousIds.insert(feature.id); const QString previousSelection=m_selectedBody; const bool opening=m_opening;
  stopWorker(); m_document=std::move(*m_candidate); m_candidate.reset(); m_committedResults=m_results; m_committedFeatures=m_pending;
  if(m_opening) { m_path=m_candidatePath; m_loadedRevision=m_document.record().revision; }
  m_dirty=!m_opening; m_opening=false; m_candidatePath.clear(); m_state=tr("Ready"); m_error.clear();
  const QVector<QString> terminals=terminalBodyIds(); QString nextSelection;
  if(opening) nextSelection=terminals.isEmpty()?QString():terminals.last();
  else { for(auto it=m_committedFeatures.crbegin(); it!=m_committedFeatures.crend(); ++it) if(!previousIds.contains(it->id) && terminals.contains(it->id)) { nextSelection=it->id; break; } if(nextSelection.isEmpty() && previousIds.contains(previousSelection) && m_committedResults.contains(previousSelection)) nextSelection=previousSelection; if(nextSelection.isEmpty() && !terminals.isEmpty()) nextSelection=terminals.last(); }
  m_selectedBody=nextSelection; updateScene();
  emit documentChanged(); emit operationStateChanged(); emit dirtyChanged();
}
void WorkspaceController::fail(const QString &message) {
  if(m_candidate) { ++m_generation; m_candidate.reset(); stopWorker(); }
  m_opening=false; m_candidatePath.clear(); m_pending.clear(); m_results.clear(); m_state=tr("Failed"); m_error=message; emit operationStateChanged();
}
void WorkspaceController::cancel() { if(!m_candidate) return; ++m_generation; m_candidate.reset(); stopWorker(); m_opening=false; m_candidatePath.clear(); m_pending.clear(); m_results.clear(); m_state=tr("Cancelled"); m_error.clear(); emit operationStateChanged(); }
void WorkspaceController::save(const QString &path) {
  if(path.isEmpty()) return; if(m_candidate) { emit saveFinished(false,tr("Wait for geometry before saving.")); return; }
  const QString target=QDir::cleanPath(QFileInfo(path).absoluteFilePath()); const bool same=!m_path.isEmpty() && target.compare(QDir::cleanPath(QFileInfo(m_path).absoluteFilePath()),Qt::CaseInsensitive)==0;
  const Result r=DocumentStorage::save(target,m_document.record(),same?std::optional<quint64>(m_loadedRevision):std::nullopt);
  if(!r.ok) { m_error=r.error; emit operationStateChanged(); emit saveFinished(false,r.error); return; }
  m_path=target; m_loadedRevision=m_document.record().revision; m_dirty=false; emit documentChanged(); emit dirtyChanged(); emit saveFinished(true,tr("Saved"));
}
void WorkspaceController::open(const QString &path) {
  if(path.isEmpty() || m_candidate) return; DocumentRecord record; const Result r=DocumentStorage::recover(path,&record);
  if(!r.ok) { fail(r.error); return; } if(record.units!="mm") { fail(tr("Only millimetre documents are supported. Convert units before opening.")); return; }
  try { Document candidate(record); m_candidatePath=QFileInfo(path).absoluteFilePath(); m_opening=true; regenerate(std::move(candidate)); } catch(const std::exception &e) { fail(QString::fromUtf8(e.what())); }
}
void WorkspaceController::newDocument() { if(m_candidate) return; ++m_generation; m_document=Document(DocumentRecord{kDocumentSchemaVersion,QUuid::createUuid().toString(QUuid::WithoutBraces),0,QStringLiteral("mm"),{}}); m_path.clear();m_loadedRevision=0;m_dirty=false;m_committedResults.clear();m_committedFeatures.clear();selectBody({});m_state=tr("Ready");m_error.clear();emit documentChanged();emit dirtyChanged();emit operationStateChanged(); }
