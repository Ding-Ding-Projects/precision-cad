#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QIcon>
#include <QStandardPaths>
#include "workspace_controller.h"
#include "ui_text.h"
#include "mesh_canvas.h"
#include "preferences_store.h"
#include "personal_vocabulary_store.h"
#include "layout_audit.h"

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv); app.setApplicationName(QStringLiteral("Precision CAD")); app.setOrganizationName(QStringLiteral("Precision CAD")); app.setApplicationVersion(QStringLiteral(PRECISION_CAD_VERSION)); app.setWindowIcon(QIcon(QStringLiteral(":/precision-cad/precision-cad.ico")));
  QCommandLineParser parser; parser.addOption({"profile-directory", "Owned profile directory for isolated runs.", "path"}); parser.addOption({"geometry-worker", "Absolute geometry worker executable for developer or test runs.", "path"}); parser.process(app);
  const QString profile=parser.value("profile-directory"); if(!profile.isEmpty()) { QDir().mkpath(profile); qputenv("PRECISION_CAD_PROFILE_DIRECTORY", profile.toUtf8()); }
  if(parser.isSet("geometry-worker")) { const QFileInfo worker(parser.value("geometry-worker")); if(!worker.isAbsolute() || !worker.isExecutable()) return 2; qputenv("PRECISION_GEOMETRY_WORKER", worker.absoluteFilePath().toUtf8()); }
  qmlRegisterType<MeshCanvas>("PrecisionCad", 1, 0, "MeshCanvas"); WorkspaceController workspace; const QString preferencesPath=profile.isEmpty()?QString():profile+"/preferences.json"; precision::preferences::PreferencesStore preferences(preferencesPath, &app); precision::preferences::PersonalVocabularyStore vocabulary(profile.isEmpty()?QString():profile+"/vocabulary.json", &app);
  UiText uiText(&preferences,&vocabulary,&app); QQmlApplicationEngine engine; engine.rootContext()->setContextProperty("uiText", &uiText); engine.rootContext()->setContextProperty("workspace", &workspace); engine.rootContext()->setContextProperty("preferences", &preferences); engine.rootContext()->setContextProperty("vocabulary", &vocabulary); engine.rootContext()->setContextProperty("buildVersion", QStringLiteral(PRECISION_CAD_VERSION)); engine.rootContext()->setContextProperty("buildTime", QStringLiteral(PRECISION_CAD_BUILD_TIME));
  const QDateTime recordedBuild = QDateTime::fromString(QStringLiteral(PRECISION_CAD_BUILD_TIME), Qt::ISODate);
  engine.rootContext()->setContextProperty("buildTime", recordedBuild.isValid() ? recordedBuild.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t")) : QStringLiteral("Unavailable"));
  const QString documentFolder = profile.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : QDir(profile).filePath(QStringLiteral("documents"));
  if (!profile.isEmpty() && !QDir().mkpath(documentFolder)) return 2;
  engine.rootContext()->setContextProperty("initialDocumentFolder", QUrl::fromLocalFile(documentFolder));
  engine.loadFromModule("PrecisionCad", "Main"); if(engine.rootObjects().isEmpty()) return 1;
  if(qEnvironmentVariableIsSet("PRECISION_LAYOUT_AUDIT")) {
    if(profile.isEmpty())return 2;
    auto *window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());if(!window)return 2;
    startLayoutAudit(window,QDir(profile).filePath(QStringLiteral("layout-audit.json")));
  }
  return app.exec();
}
