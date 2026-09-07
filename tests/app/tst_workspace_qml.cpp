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
};
int main(int argc,char **argv) { qputenv("QT_QPA_PLATFORM","offscreen"); qputenv("QT_QUICK_CONTROLS_STYLE","Material"); QGuiApplication app(argc,argv); qmlRegisterType<MeshCanvas>("PrecisionCad",1,0,"MeshCanvas"); WorkspaceQmlTest test; return QTest::qExec(&test,argc,argv); }
#include "tst_workspace_qml.moc"
