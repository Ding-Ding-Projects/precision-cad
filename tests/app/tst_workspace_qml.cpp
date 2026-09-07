#include <QtTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QTemporaryDir>
#include "workspace_controller.h"
#include "mesh_canvas.h"
#include "ui_text.h"
class ModelRowWorkspace final : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList features READ features CONSTANT)
 Q_PROPERTY(QVariantList meshVertices READ meshVertices CONSTANT)
 Q_PROPERTY(QVariantList meshIndices READ meshIndices CONSTANT)
 Q_PROPERTY(QString operationState READ operationState CONSTANT)
 Q_PROPERTY(QString errorMessage READ errorMessage CONSTANT)
 Q_PROPERTY(QString volume READ volume CONSTANT)
 Q_PROPERTY(QString bounds READ bounds CONSTANT)
 Q_PROPERTY(bool dirty READ dirty CONSTANT)
 Q_PROPERTY(bool busy READ busy CONSTANT)
public:
 QVariantList features() const { return QVariantList{QVariantMap{{"id", "box-feature-id-12345678"}, {"label", "Box feature with a deliberately long visible model name"}, {"suppressed", false}}}; }
 QVariantList meshVertices() const { return {}; } QVariantList meshIndices() const { return {}; }
 QString operationState() const { return "Ready"; } QString errorMessage() const { return {}; } QString volume() const { return "24000 mm³"; } QString bounds() const { return "[-1e-07, -1e-07, -1e-07] to [40, 30, 20] mm"; } bool dirty() const { return false; } bool busy() const { return false; }
 Q_INVOKABLE void selectBody(const QString &) {} Q_INVOKABLE QString localPath(const QUrl &url) const { return url.toLocalFile(); }
 Q_INVOKABLE QVariantMap editableDimensions(const QString &) const { return {{"editable", false}}; }
 Q_INVOKABLE void addBox(double,double,double) {} Q_INVOKABLE void addCylinder(double,double) {} Q_INVOKABLE void booleanOperation(const QString &,const QString &,const QString &) {} Q_INVOKABLE void suppressFeature(const QString &,bool) {} Q_INVOKABLE void updateDimensions(const QString &,double,double,double=0) {} Q_INVOKABLE void undo() {} Q_INVOKABLE void redo() {} Q_INVOKABLE void cancel() {} Q_INVOKABLE void save(const QString &) {} Q_INVOKABLE void open(const QString &) {} Q_INVOKABLE void newDocument() {}
signals:
 void saveFinished(bool ok, const QString &message);
};
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
   QVERIFY(prefs.setLanguageMode(language)); QVERIFY(prefs.setTheme(theme)); window->resize(size); window->show(); QCoreApplication::processEvents(); QTest::qWait(20); QCoreApplication::processEvents();
   QVERIFY2(qAbs(toolbar->height()-(flow->implicitHeight()+toolbar->property("topPadding").toReal()+toolbar->property("bottomPadding").toReal())) < 0.1, "toolbar height must include the visible Flow and its native vertical insets");
   const QRectF toolbarRect=toolbar->mapRectToScene(toolbar->boundingRect());
   const QRectF workspaceRect=workspaceSplit->mapRectToScene(workspaceSplit->boundingRect());
   QVERIFY2(qAbs(workspaceRect.top()-toolbarRect.bottom()) < 0.1, qPrintable(QStringLiteral("workspace must begin below the toolbar, workspace top=%1 toolbar bottom=%2").arg(workspaceRect.top()).arg(toolbarRect.bottom())));
   QVERIFY2(workspaceSplit->height() >= 400, "supported minimum must retain a usable workspace height");
   QCOMPARE(flow->childItems().size(), 13);
   for (auto *control : flow->childItems()) { const QRectF controlRect=control->mapRectToScene(control->boundingRect()); QVERIFY2(controlRect.top() >= toolbarRect.top()-0.1 && controlRect.bottom() <= toolbarRect.bottom()+0.1, "every toolbar control must remain inside the measured toolbar"); }
   QVERIFY2(inspector->childrenRect().width() <= inspector->width()+0.1, "inspector children must stay within their constrained column");
   QVERIFY2(notice->width() <= inspector->width()+0.1, "long inspector notice must wrap inside the pane");
   QVERIFY2(help->mapRectToScene(help->boundingRect()).bottom() <= modelColumn->mapRectToScene(modelColumn->boundingRect()).bottom()+0.1, "model selection help must remain reachable at the model-pane bottom");
  }
  QVERIFY(prefs.setLanguageMode("en")); QVERIFY(prefs.setFontScale(1.0)); QVERIFY(prefs.setTheme("light")); window->resize(1280,820); QCoreApplication::processEvents(); QTest::qWait(20); QCoreApplication::processEvents();
  QVERIFY2(version->mapRectToScene(version->boundingRect()).intersects(inspectorScroll->mapRectToScene(inspectorScroll->boundingRect())), "version provenance must be initially visible at the default client area");
  QVERIFY(prefs.setLanguageMode("both")); QVERIFY(prefs.setEnglishTone(5)); QVERIFY(prefs.setCantoneseTone(5)); QVERIFY(prefs.setFontScale(1.5)); QVERIFY(prefs.setTheme("dark")); window->resize(800,600); QCoreApplication::processEvents(); QTest::qWait(20); QCoreApplication::processEvents();
  QVERIFY2(qAbs(toolbar->height()-(flow->implicitHeight()+toolbar->property("topPadding").toReal()+toolbar->property("bottomPadding").toReal())) < 0.1, "high-content toolbar must include its visible Flow and native vertical insets");
  const QRectF highToolbarRect=toolbar->mapRectToScene(toolbar->boundingRect()); QCOMPARE(flow->childItems().size(), 13);
  const QRectF highWorkspaceRect=workspaceSplit->mapRectToScene(workspaceSplit->boundingRect());
  QVERIFY2(qAbs(highWorkspaceRect.top()-highToolbarRect.bottom()) < 0.1 && workspaceSplit->height() >= 300, "high-content minimum must retain a positive workspace below the toolbar");
  for (auto *control : flow->childItems()) { const QRectF controlRect=control->mapRectToScene(control->boundingRect()); QVERIFY2(controlRect.top() >= highToolbarRect.top()-0.1 && controlRect.bottom() <= highToolbarRect.bottom()+0.1, "every high-content toolbar control must remain inside the measured toolbar"); }
  QVERIFY2(help->mapRectToScene(help->boundingRect()).bottom() <= modelColumn->mapRectToScene(modelColumn->boundingRect()).bottom()+0.1, "high-content model selection help must remain reachable");
  const QRectF scrollRect=inspectorScroll->mapRectToScene(inspectorScroll->boundingRect());
  QObject *flickable=inspectorScroll->property("contentItem").value<QObject*>(); QVERIFY2(flickable, "ScrollView must expose its real content item");
  QVERIFY2(flickable->metaObject()->indexOfProperty("contentY") >= 0 && flickable->metaObject()->indexOfProperty("contentHeight") >= 0, "ScrollView content item must expose real scrolling properties");
  const qreal maximumContentY=qMax(0.0, flickable->property("contentHeight").toReal()-flickable->property("height").toReal());
  if (maximumContentY > 0.1) {
   QVERIFY2(flickable->setProperty("contentY", maximumContentY), "setting the real Flickable contentY must succeed"); QCoreApplication::processEvents();
   QVERIFY2(qAbs(flickable->property("contentY").toReal()-maximumContentY) < 0.1, "the real Flickable must reach its maximum contentY");
   QVERIFY2(notice->mapRectToScene(notice->boundingRect()).bottom() <= scrollRect.bottom()+0.1, "scrolling to the inspector bottom must reach the final notice");
  } else {
   QQmlComponent overflowFixture(&engine); overflowFixture.setData("import QtQuick; import QtQuick.Controls; ScrollView { objectName: \"fixtureScroll\"; width: 200; height: 100; contentWidth: availableWidth; Column { objectName: \"fixtureColumn\"; width: parent.width; Repeater { model: 10; delegate: Rectangle { width: 200; height: 20 } } } }", QUrl());
   std::unique_ptr<QObject> fixtureRoot(overflowFixture.create()); QVERIFY2(fixtureRoot!=nullptr,qPrintable(overflowFixture.errorString())); QCoreApplication::processEvents();
   QObject *fixtureFlickable=fixtureRoot->property("contentItem").value<QObject*>(); QVERIFY(fixtureFlickable); const qreal fixtureMaximum=qMax(0.0, fixtureFlickable->property("contentHeight").toReal()-fixtureFlickable->property("height").toReal()); QVERIFY2(fixtureMaximum > 0.1, "isolated native scroll fixture must overflow before proving reachability");
   QVERIFY(fixtureFlickable->setProperty("contentY", fixtureMaximum)); QCOMPARE(fixtureFlickable->property("contentY").toReal(), fixtureMaximum);
  }
 }
 void populatedModelRowGeometry() {
  QTemporaryDir dir; QVERIFY(dir.isValid());
  precision::preferences::PreferencesStore prefs(dir.filePath("prefs.json"));
  precision::preferences::PersonalVocabularyStore vocab(dir.filePath("private-cache.json"));
  ModelRowWorkspace workspace;
  UiText ui(&prefs,&vocab); QQmlEngine engine; auto context=engine.rootContext(); context->setContextProperty("preferences",&prefs); context->setContextProperty("vocabulary",&vocab); context->setContextProperty("workspace",&workspace); context->setContextProperty("uiText",&ui); context->setContextProperty("buildVersion","test"); context->setContextProperty("buildTime","unavailable");
  QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(MAIN_QML_PATH))); QVERIFY2(component.isReady(),qPrintable(component.errorString())); std::unique_ptr<QObject> root(component.create()); QVERIFY2(root!=nullptr,qPrintable(component.errorString())); auto *window=qobject_cast<QQuickWindow*>(root.get()); QVERIFY(window); window->resize(800,600); window->show(); QCoreApplication::processEvents(); QTest::qWait(20); QCoreApplication::processEvents();
  auto *tree=root->findChild<QQuickItem*>("modelTree"); QVERIFY(tree); tree->setProperty("currentIndex",0); QMetaObject::invokeMethod(tree,"forceLayout"); QTRY_COMPARE_WITH_TIMEOUT(tree->property("count").toInt(), 1, 1000); QTRY_VERIFY_WITH_TIMEOUT(tree->property("currentItem").value<QObject*>() != nullptr, 1000); auto *delegate=qobject_cast<QQuickItem*>(tree->property("currentItem").value<QObject*>()); QVERIFY(delegate); auto *row=delegate->findChild<QQuickItem*>("modelRow"); auto *label=delegate->findChild<QQuickItem*>("modelRowLabel"); auto *includeSwitch=delegate->findChild<QQuickItem*>("modelRowIncludeSwitch"); QVERIFY(row); QVERIFY(label); QVERIFY(includeSwitch); QVERIFY(includeSwitch->isVisible());
  const QRectF treeRect=tree->mapRectToScene(tree->boundingRect()); const QRectF rowRect=row->mapRectToScene(row->boundingRect()); const QRectF labelRect=label->mapRectToScene(label->boundingRect()); const QRectF switchRect=includeSwitch->mapRectToScene(includeSwitch->boundingRect());
  QVERIFY2(rowRect.left() >= treeRect.left()-0.1 && rowRect.right() <= treeRect.right()+0.1, "populated model row must fit in the clipped model tree");
  QVERIFY2(labelRect.left() >= rowRect.left()-0.1 && labelRect.right() <= switchRect.left()-0.1, "model text must reserve measured space for the Include switch");
  QVERIFY2(switchRect.right() <= rowRect.right()+0.1 && includeSwitch->width() >= includeSwitch->implicitWidth()-0.1, "the full Include switch must remain inside the populated row");
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
