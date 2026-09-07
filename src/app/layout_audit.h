#pragma once
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <functional>
#include <atomic>

inline std::atomic<int> auditWarningCount{0};
inline std::atomic<int> auditCriticalCount{0};
inline std::atomic<int> auditPlacementWarnings{0};
inline std::atomic<int> auditBindingWarnings{0};
inline std::atomic<int> auditOtherWarnings{0};
inline void enableLayoutAuditMessages() {
 qInstallMessageHandler([](QtMsgType type,const QMessageLogContext &,const QString &message){
  if(type==QtWarningMsg){++auditWarningCount;
   if(message.startsWith(QStringLiteral("QWindowsWindow::setGeometry")))++auditPlacementWarnings;
   else if(message.contains(QStringLiteral("Binding loop"),Qt::CaseInsensitive))++auditBindingWarnings;
   else ++auditOtherWarnings;
  }
  if(type==QtCriticalMsg||type==QtFatalMsg)++auditCriticalCount;
 });
}

// Diagnostic geometry only: never serialize text, model data, settings or file paths.
inline void startLayoutAudit(QQuickWindow *window, const QString &outputPath) {
 auto capture=[window,outputPath] {
  const QSet<QString> allowed={"mainToolbar","toolbarFlow","brandLabel","workspaceSplit","modelPane","modelColumn","modelTree","inspectorPane","inspectorColumn","versionInfo","rawDiagnostics","viewport","settingsPanel","selectionHelp"};
  const auto rect=[](QRectF r){return QJsonObject{{"x",r.x()},{"y",r.y()},{"width",r.width()},{"height",r.height()},{"right",r.right()},{"bottom",r.bottom()}};};
  QJsonArray elements; int visited=0; bool truncated=false;
  std::function<void(QQuickItem*,int)> walk=[&](QQuickItem *item,int depth){
   if(++visited>5000||depth>64){truncated=true;return;}
   if(allowed.contains(item->objectName())) elements.append(QJsonObject{{"id",item->objectName()},{"type",QString::fromLatin1(item->metaObject()->className())},{"rect",rect(item->mapRectToScene(item->boundingRect()))},{"childrenRect",rect(item->childrenRect())},{"implicitWidth",item->implicitWidth()},{"implicitHeight",item->implicitHeight()},{"visible",item->isVisible()},{"opacity",item->opacity()},{"clipsChildren",item->clip()}});
   for(auto *child:item->childItems()){if(truncated)break;walk(child,depth+1);}
  };
  walk(window->contentItem(),0);
#ifdef PRECISION_CAD_SOURCE_COMMIT
  const QString source=QStringLiteral(PRECISION_CAD_SOURCE_COMMIT);
#else
  const QString source=QStringLiteral("unavailable");
#endif
  const QJsonObject result{{"version",1},{"kind","qtquick-native-layout"},{"capturedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"sourceCommit",source},{"processId",QCoreApplication::applicationPid()},{"windowHandle",QString::number(static_cast<qulonglong>(window->winId()))},{"viewport",QJsonObject{{"width",window->width()},{"height",window->height()},{"scale",window->devicePixelRatio()}}},{"visitedItems",visited},{"truncated",truncated},{"runtime",QJsonObject{{"warningCount",auditWarningCount.load()},{"criticalCount",auditCriticalCount.load()},{"placementWarningCount",auditPlacementWarnings.load()},{"bindingWarningCount",auditBindingWarnings.load()},{"otherWarningCount",auditOtherWarnings.load()}}},{"elements",elements}};
  QSaveFile file(outputPath); const auto bytes=QJsonDocument(result).toJson(QJsonDocument::Compact);
  if(file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size())file.commit();
 };
 auto *timer=new QTimer(window);timer->setInterval(1000);QObject::connect(timer,&QTimer::timeout,window,capture);timer->start();QTimer::singleShot(300,window,capture);
}
