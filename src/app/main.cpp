#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "workspace_controller.h"
#include "mesh_canvas.h"

int main(int argc, char *argv[]) {
  QGuiApplication app(argc, argv); app.setApplicationName(QStringLiteral("Precision CAD")); app.setOrganizationName(QStringLiteral("Precision CAD")); app.setApplicationVersion(QStringLiteral(PRECISION_CAD_VERSION));
  qmlRegisterType<MeshCanvas>("PrecisionCad", 1, 0, "MeshCanvas"); WorkspaceController workspace;
  QQmlApplicationEngine engine; engine.rootContext()->setContextProperty("workspace", &workspace); engine.rootContext()->setContextProperty("buildVersion", QStringLiteral(PRECISION_CAD_VERSION)); engine.rootContext()->setContextProperty("buildTime", QStringLiteral(PRECISION_CAD_BUILD_TIME));
  engine.loadFromModule("PrecisionCad", "Main"); if(engine.rootObjects().isEmpty()) return 1; return app.exec();
}
