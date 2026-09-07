#include <QtTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickWindow>
#include <QTemporaryDir>
#include "workspace_controller.h"
#include "mesh_canvas.h"
#include "ui_text.h"
class WorkspaceQmlTest final : public QObject {
 Q_OBJECT
private slots:
 void surfaceBindings() {
  QTemporaryDir dir; QVERIFY(dir.isValid());
  precision::preferences::PreferencesStore prefs(dir.filePath("prefs.json"));
  precision::preferences::PersonalVocabularyStore vocab(dir.filePath("private-cache.json"));
  WorkspaceController workspace; UiText ui(&prefs,&vocab); QQmlEngine engine;
  auto context=engine.rootContext(); context->setContextProperty("preferences",&prefs); context->setContextProperty("vocabulary",&vocab); context->setContextProperty("workspace",&workspace); context->setContextProperty("uiText",&ui); context->setContextProperty("buildVersion","test"); context->setContextProperty("buildTime","unavailable");
  QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(MAIN_QML_PATH))); QVERIFY2(component.isReady(),qPrintable(component.errorString()));
  std::unique_ptr<QObject> root(component.create()); QVERIFY2(root!=nullptr,qPrintable(component.errorString()));
  auto diagnostics=root->findChild<QObject*>("rawDiagnostics"); QVERIFY(diagnostics); QCOMPARE(diagnostics->property("textFormat").toInt(),int(Qt::PlainText));
  auto help=root->findChild<QObject*>("selectionHelp"); QVERIFY(help); auto language=root->findChild<QObject*>("languageMode"); QVERIFY(language);
  QVERIFY(prefs.setLanguageMode("en")); QVERIFY(prefs.setEnglishTone(1)); QCoreApplication::processEvents(); const auto serious=help->property("text").toString();
  QSet<QString> variants; for(int tone=1;tone<=5;++tone) { QVERIFY(prefs.setEnglishTone(tone)); QCoreApplication::processEvents(); variants.insert(help->property("text").toString()); } QCOMPARE(variants.size(),5);
  QVERIFY(prefs.setEnglishTone(5)); QCoreApplication::processEvents(); QVERIFY(help->property("text").toString()!=serious);
  QVERIFY(prefs.setLanguageMode("yue")); QCoreApplication::processEvents(); QCOMPARE(language->property("currentIndex").toInt(),1); const auto playfulYue=help->property("text").toString();
  variants.clear(); for(int tone=1;tone<=5;++tone) { QVERIFY(prefs.setCantoneseTone(tone)); QCoreApplication::processEvents(); variants.insert(help->property("text").toString()); } QCOMPARE(variants.size(),5);
  QVERIFY(prefs.setCantoneseTone(1)); QCoreApplication::processEvents(); QVERIFY(help->property("text").toString()!=playfulYue);
  QVERIFY(prefs.setLanguageMode("both")); QCoreApplication::processEvents(); QCOMPARE(language->property("currentIndex").toInt(),2); QVERIFY(help->property("text").toString().contains('\n'));
  QVERIFY(prefs.setTheme("light")); QCoreApplication::processEvents(); QCOMPARE(root->property("effectiveTheme").toInt(),0); auto viewport=root->findChild<QObject*>("viewport"); QVERIFY(viewport); const auto lightBackground=viewport->property("backgroundColor");
  QVERIFY(prefs.setTheme("dark")); QCoreApplication::processEvents(); QCOMPARE(root->property("effectiveTheme").toInt(),1); QVERIFY(viewport->property("backgroundColor")!=lightBackground);
  root->setProperty("deferredAction","new"); root->setProperty("saveForDeferred",true); auto save=root->findChild<QObject*>("saveDialog"); QVERIFY(save); QVERIFY(QMetaObject::invokeMethod(save,"rejected")); QCOMPARE(root->property("deferredAction").toString(),QString()); QVERIFY(!root->property("saveForDeferred").toBool());
  root->setProperty("deferredAction","new"); root->setProperty("saveForDeferred",true); workspace.saveFinished(false,"failure"); QCOMPARE(root->property("deferredAction").toString(),QString()); QVERIFY(!root->property("saveForDeferred").toBool());
  QCOMPARE(workspace.localPath(QUrl("https://example.invalid/document")),QString()); QCOMPARE(workspace.localPath(QUrl::fromLocalFile(dir.filePath("a b.pcad"))),dir.filePath("a b.pcad"));
 }
 void nativeLayoutGeometry() {
  QTemporaryDir dir; QVERIFY(dir.isValid());
  precision::preferences::PreferencesStore prefs(dir.filePath("prefs.json"));
  precision::preferences::PersonalVocabularyStore vocab(dir.filePath("private-cache.json"));
  WorkspaceController workspace; UiText ui(&prefs,&vocab); QQmlEngine engine;
  auto context=engine.rootContext(); context->setContextProperty("preferences",&prefs); context->setContextProperty("vocabulary",&vocab); context->setContextProperty("workspace",&workspace); context->setContextProperty("uiText",&ui); context->setContextProperty("buildVersion","test-version-with-provenance"); context->setContextProperty("buildTime","2026-09-07 14:34:01 Eastern Daylight Time");
  QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(MAIN_QML_PATH))); QVERIFY2(component.isReady(),qPrintable(component.errorString()));
  std::unique_ptr<QObject> root(component.create()); QVERIFY2(root!=nullptr,qPrintable(component.errorString()));
  auto *window=qobject_cast<QQuickWindow*>(root.get()); QVERIFY(window);
  auto *toolbar=root->findChild<QQuickItem*>("mainToolbar"); auto *flow=root->findChild<QQuickItem*>("toolbarFlow"); auto *workspaceSplit=root->findChild<QQuickItem*>("workspaceSplit"); auto *modelColumn=root->findChild<QQuickItem*>("modelColumn"); auto *help=root->findChild<QQuickItem*>("selectionHelp"); auto *inspectorScroll=root->findChild<QQuickItem*>("inspectorScroll"); auto *inspector=root->findChild<QQuickItem*>("inspectorColumn"); auto *version=root->findChild<QQuickItem*>("versionInfo"); auto *notice=root->findChild<QQuickItem*>("inspectorNotice");
  QVERIFY(toolbar); QVERIFY(flow); QVERIFY(workspaceSplit); QVERIFY(modelColumn); QVERIFY(help); QVERIFY(inspectorScroll); QVERIFY(inspector); QVERIFY(version); QVERIFY(notice);
  const QList<QSize> sizes{{1280, 820}, {1024, 700}, {800, 600}};
  const QStringList languages{"en", "yue", "both"};
  const QStringList themes{"light", "dark"};
  for (const auto &size : sizes) for (const auto &language : languages) for (const auto &theme : themes) {
   QVERIFY(prefs.setLanguageMode(language)); QVERIFY(prefs.setTheme(theme)); window->resize(size); window->show(); QCoreApplication::processEvents(); QCoreApplication::processEvents();
   QVERIFY2(qAbs(toolbar->height()-flow->implicitHeight()) < 0.1, "toolbar height must match the visible Flow height");
   QVERIFY2(qAbs(workspaceSplit->y()-toolbar->height()) < 0.1, "workspace must begin below the toolbar");
   QVERIFY2(workspaceSplit->height() >= 400, "supported minimum must retain a usable workspace height");
   const QRectF toolbarRect=toolbar->mapRectToScene(toolbar->boundingRect());
   QCOMPARE(flow->childItems().size(), 13);
   for (auto *control : flow->childItems()) { const QRectF controlRect=control->mapRectToScene(control->boundingRect()); QVERIFY2(controlRect.top() >= toolbarRect.top()-0.1 && controlRect.bottom() <= toolbarRect.bottom()+0.1, "every toolbar control must remain inside the measured toolbar"); }
   QVERIFY2(inspector->childrenRect().width() <= inspector->width()+0.1, "inspector children must stay within their constrained column");
   QVERIFY2(notice->width() <= inspector->width()+0.1, "long inspector notice must wrap inside the pane");
   QVERIFY2(help->mapRectToScene(help->boundingRect()).bottom() <= modelColumn->mapRectToScene(modelColumn->boundingRect()).bottom()+0.1, "model selection help must remain reachable at the model-pane bottom");
  }
  QVERIFY(prefs.setLanguageMode("both")); QVERIFY(prefs.setEnglishTone(5)); QVERIFY(prefs.setCantoneseTone(5)); QVERIFY(prefs.setFontScale(1.5)); QVERIFY(prefs.setTheme("dark")); window->resize(800,600); QCoreApplication::processEvents(); QCoreApplication::processEvents();
  const QRectF scrollRect=inspectorScroll->mapRectToScene(inspectorScroll->boundingRect());
  QVERIFY2(version->mapRectToScene(version->boundingRect()).intersects(scrollRect), "version provenance must be initially visible at the constrained minimum");
  inspectorScroll->setProperty("contentY", inspectorScroll->property("contentHeight").toReal()-inspectorScroll->height()); QCoreApplication::processEvents();
  QVERIFY2(notice->mapRectToScene(notice->boundingRect()).bottom() <= scrollRect.bottom()+0.1, "scrolling to the inspector bottom must reach the final notice");
 }
 void legacyGeometryFixturesFail() {
  QQmlEngine engine;
  QQmlComponent toolbarFixture(&engine); toolbarFixture.setData("import QtQuick; Item { width: 800; height: 600; Item { objectName: \"legacyToolbar\"; height: 16; Item { objectName: \"legacyFlow\"; implicitHeight: 128; width: 800; height: 128 } } }", QUrl());
  std::unique_ptr<QObject> toolbarRoot(toolbarFixture.create()); QVERIFY(toolbarRoot);
  auto *legacyToolbar=toolbarRoot->findChild<QQuickItem*>("legacyToolbar"); auto *legacyFlow=toolbarRoot->findChild<QQuickItem*>("legacyFlow"); QVERIFY(legacyToolbar); QVERIFY(legacyFlow);
  QEXPECT_FAIL("", "The old toolbar fixture must fail the measured height contract.", Continue); QVERIFY(qAbs(legacyToolbar->height()-legacyFlow->implicitHeight()) < 0.1);
  QQmlComponent inspectorFixture(&engine); inspectorFixture.setData("import QtQuick; Item { width: 200; height: 100; Column { objectName: \"legacyColumn\"; Repeater { model: 10; delegate: Rectangle { width: 200; height: 20 } } } }", QUrl());
  std::unique_ptr<QObject> inspectorRoot(inspectorFixture.create()); QVERIFY(inspectorRoot);
  auto *legacyColumn=inspectorRoot->findChild<QQuickItem*>("legacyColumn"); QVERIFY(legacyColumn);
  QEXPECT_FAIL("", "The old non-scrollable inspector fixture must fail its reachable-content contract.", Continue); QVERIFY(legacyColumn->childrenRect().height() <= 100);
 }
};
int main(int argc,char **argv) { qputenv("QT_QPA_PLATFORM","offscreen"); qputenv("QT_QUICK_CONTROLS_STYLE","Material"); QGuiApplication app(argc,argv); qmlRegisterType<MeshCanvas>("PrecisionCad",1,0,"MeshCanvas"); WorkspaceQmlTest test; return QTest::qExec(&test,argc,argv); }
#include "tst_workspace_qml.moc"
