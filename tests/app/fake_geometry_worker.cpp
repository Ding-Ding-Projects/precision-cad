#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QFile>

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv); QFile input; input.open(stdin,QIODevice::ReadOnly); QFile output; output.open(stdout,QIODevice::WriteOnly); const QJsonObject request=QJsonDocument::fromJson(input.readAll()).object(); const double dx=request.value("parameters").toObject().value("dx").toDouble();
  if(dx==11) { output.write("{"); return 0; }
  if(dx==12) { QJsonObject reply{{"ok",true},{"operationId","wrong"},{"documentId",request.value("documentId")},{"revision",request.value("revision")},{"result",QJsonObject{}}}; output.write(QJsonDocument(reply).toJson(QJsonDocument::Compact)); return 0; }
  if(dx==13) { output.write(QByteArray(9*1024*1024,'x')); return 0; }
  if(dx==14) { QThread::sleep(50); return 0; }
  const QJsonObject mesh{{"vertices",QJsonArray{0,0,0,1,0,0,0,1,0}},{"indices",QJsonArray{0,1,2}}}; const QJsonObject result{{"brep","fake"},{"valid",true},{"volume",1.0},{"bounds",QJsonArray{0,0,0,1,1,1}},{"mesh",mesh}};
  const QJsonObject reply{{"ok",true},{"operationId",request.value("operationId")},{"documentId",request.value("documentId")},{"revision",request.value("revision")},{"result",result}}; output.write(QJsonDocument(reply).toJson(QJsonDocument::Compact)); return 0;
}
