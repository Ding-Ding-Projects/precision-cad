#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QFile>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);

 QFile input; input.open(stdin,QIODevice::ReadOnly); QFile output; output.open(stdout,QIODevice::WriteOnly); const QJsonObject request=QJsonDocument::fromJson(input.readAll()).object(); const double dx=request.value("parameters").toObject().value("dx").toDouble();
#ifdef Q_OS_WIN
  BOOL inJob=FALSE; JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
  if(!IsProcessInJob(GetCurrentProcess(),nullptr,&inJob) || !inJob || !QueryInformationJobObject(nullptr,JobObjectExtendedLimitInformation,&limit,sizeof(limit),nullptr) || limit.JobMemoryLimit!=512ull*1024ull*1024ull || limit.BasicLimitInformation.ActiveProcessLimit!=1) return 9;
#endif
  if(dx<0) return 1;
  if(dx==11) { output.write("{"); return 0; }
  if(dx==112) { QJsonObject reply{{"ok",true},{"operationId","wrong"},{"documentId",request.value("documentId")},{"revision",request.value("revision")},{"result",QJsonObject{}}}; output.write(QJsonDocument(reply).toJson(QJsonDocument::Compact)); return 0; }
  if(dx==13) { output.write(QByteArray(9*1024*1024,'x')); return 0; }
  if(dx==14) { QThread::sleep(50); return 0; }
  if(dx==15) { QFile errors; errors.open(stderr,QIODevice::WriteOnly); errors.write(QByteArray(9*1024*1024,'x')); errors.flush(); QThread::sleep(5); return 0; }
  if(dx==16) QThread::msleep(200);
  QJsonObject mesh{{"vertices",QJsonArray{0,0,0,1,0,0,0,1,0}},{"indices",QJsonArray{0,1,2}},{"normals",QJsonArray{0,0,1,0,0,1,0,0,1}},{"absoluteDeflection",0.001},{"targetRelativeDeflection",0.001},{"effectiveRelativeDeflection",0.001}}; if(dx==19) mesh.insert("indices",QJsonArray{0,1,999});
  if(dx==20) mesh.insert("vertices",QJsonArray{});
  QJsonObject result{{"brep","fake"},{"valid",true},{"volume",1.0},{"bounds",QJsonArray{0,0,0,1,1,1}},{"mesh",mesh}};
  if(dx==26 || dx==27) { mesh.insert("vertices",QJsonArray{dx==26?1.0e9:1.0e9+1,0,0,1,0,0,0,1,0}); result.insert("mesh",mesh); }
  if(dx==28 || dx==29) result.insert("bounds",QJsonArray{-1.0e9,0,0,dx==28?1.0e9:1.0e9+1,1,1});
  if(dx==30) result.insert("brep",QString(4000,'b'));
  if(dx==21) result.insert("volume",QJsonValue());
  if(dx==22) result.insert("bounds",QJsonArray{2,0,0,1,1,1});
  if(dx==23) result.insert("valid",false);
  if(dx==18) result.insert("brep",QString());
  QJsonObject reply{{"protocolVersion",dx==17?2:1},{"ok",true},{"operationId",request.value("operationId")},{"documentId",request.value("documentId")},{"revision",request.value("revision")},{"result",result}}; if(dx==12) reply.insert("operationId","wrong"); if(dx==24) reply.insert("revision",999); if(dx==25) reply.insert("documentId","wrong"); output.write(QJsonDocument(reply).toJson(QJsonDocument::Compact)); return 0;
}
