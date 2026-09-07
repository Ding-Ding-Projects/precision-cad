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
#include "viewport/viewport_camera.h"
#include "viewport/mesh_geometry.h"
#include <limits>
#include <cstring>
class ModelRowWorkspace final : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList meshParts MEMBER sceneParts NOTIFY meshChanged)
 Q_PROPERTY(QVariantList meshNormals MEMBER sceneNormals NOTIFY meshChanged)
 Q_PROPERTY(QVariantList features READ features NOTIFY documentChanged)
 Q_PROPERTY(QVariantList meshVertices READ meshVertices NOTIFY meshChanged)
 Q_PROPERTY(QVariantList meshIndices READ meshIndices NOTIFY meshChanged)
 Q_PROPERTY(QString selectedBody READ selectedBody NOTIFY selectedBodyChanged)
 Q_PROPERTY(QString operationState READ operationState CONSTANT)
 Q_PROPERTY(QString errorMessage READ errorMessage CONSTANT)
 Q_PROPERTY(QString volume READ volume CONSTANT)
 Q_PROPERTY(QString bounds READ bounds CONSTANT)
 Q_PROPERTY(bool dirty READ dirty CONSTANT)
 Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
 QVariantList sceneParts,sceneNormals;
 QVariantList rows{QVariantMap{{"id", "box-feature-id-12345678"}, {"label", "Box feature with a deliberately long visible model name"}, {"suppressed", false}}};
 QMap<QString,QVariantMap> dimensions;
 QString activeBody, updatedBody;
 QVariantList updatedDimensions;
 int updateCount=0;
 bool regenerating=false;
 QVariantList features() const { return rows; }
 QVariantList meshVertices() const { const double x=activeBody=="A" ? 10 : activeBody=="B" ? 20 : 30; return activeBody.isEmpty() ? QVariantList{} : QVariantList{x,0.,0.,x+1,0.,0.,x,1.,0.}; }
 QVariantList meshIndices() const { return activeBody.isEmpty() ? QVariantList{} : QVariantList{0,1,2}; }
 QString selectedBody() const { return activeBody; }
 QString operationState() const { return "Ready"; } QString errorMessage() const { return {}; } QString volume() const { return "24000 mm³"; } QString bounds() const { return "[-1e-07, -1e-07, -1e-07] to [40, 30, 20] mm"; } bool dirty() const { return false; } bool busy() const { return regenerating; }
 void setBusy(bool value) { regenerating=value; emit busyChanged(); }
 Q_INVOKABLE void selectBody(const QString &id) { activeBody=id; emit selectedBodyChanged(); emit meshChanged(); }
 Q_INVOKABLE QString localPath(const QUrl &url) const { return url.toLocalFile(); }
 Q_INVOKABLE QVariantMap editableDimensions(const QString &id) const { return dimensions.value(id, {{"editable", false}}); }
 Q_INVOKABLE void updateDimensions(const QString &id,double first,double second,double third=0) { ++updateCount; updatedBody=id; updatedDimensions={first,second,third}; dimensions[id]={{"editable",true},{"type","box"},{"first",first},{"second",second},{"third",third}}; }
 Q_INVOKABLE void newDocument() { rows.clear(); dimensions.clear(); selectBody({}); emit documentChanged(); }
 Q_INVOKABLE void addBox(double,double,double) {} Q_INVOKABLE void addCylinder(double,double) {} Q_INVOKABLE void booleanOperation(const QString &,const QString &,const QString &) {} Q_INVOKABLE void suppressFeature(const QString &,bool) {} Q_INVOKABLE void undo() {} Q_INVOKABLE void redo() {} Q_INVOKABLE void cancel() {} Q_INVOKABLE void save(const QString &) {} Q_INVOKABLE void open(const QString &) {}
signals:
 void saveFinished(bool ok, const QString &message);
 void documentChanged();
 void selectedBodyChanged();
 void meshChanged();
 void busyChanged();
};
class WorkspaceQmlTest final : public QObject {
 Q_OBJECT
private slots:
 void nativeCameraBindingPickingAndInput() {
  QTemporaryDir dir; QVERIFY(dir.isValid());
  precision::preferences::PreferencesStore prefs(dir.filePath("prefs.json"));
  precision::preferences::PersonalVocabularyStore vocab(dir.filePath("private-cache.json"));
  ModelRowWorkspace workspace;
  const double origin=1e12;
  const QVariantList nearVertices{origin-2,origin-2,origin+1,origin+2,origin-2,origin+1,origin,origin+2,origin+1};
  const QVariantList farVertices{origin-2,origin-2,origin,origin+2,origin-2,origin,origin,origin+2,origin};
  workspace.sceneParts={QVariantMap{{"bodyId","far"},{"vertices",farVertices},{"indices",QVariantList{0,1,2}}},QVariantMap{{"bodyId","near"},{"vertices",nearVertices},{"indices",QVariantList{0,1,2}},{"normals",QVariantList{0,0,1,0,0,1,0,0,1}}}};
  UiText ui(&prefs,&vocab); QQmlEngine engine;
  auto context=engine.rootContext(); context->setContextProperty("preferences",&prefs);context->setContextProperty("vocabulary",&vocab);context->setContextProperty("workspace",&workspace);context->setContextProperty("uiText",&ui);context->setContextProperty("buildVersion","test");context->setContextProperty("buildTime","unavailable");
  QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(MAIN_QML_PATH)));
  QVERIFY2(component.isReady(),qPrintable(component.errorString()));
  std::unique_ptr<QObject> root(component.create()); QVERIFY2(root!=nullptr,qPrintable(component.errorString()));
  auto *window=qobject_cast<QQuickWindow*>(root.get());QVERIFY(window);
  auto *viewport=root->findChild<QQuickItem*>("viewport");QVERIFY(viewport);
  auto *camera=root->findChild<precision::app::viewport::ViewportCamera*>("cameraController");QVERIFY(camera);
  auto *mesh=root->findChild<precision::app::viewport::MeshGeometry*>("sceneMesh");QVERIFY(mesh);
  auto *native=root->findChild<QQuickItem*>("nativeView");QVERIFY(native);
  auto *perspective=root->findChild<QObject*>("perspectiveCamera");auto *orthographic=root->findChild<QObject*>("orthographicCamera");QVERIFY(perspective);QVERIFY(orthographic);
  window->show();QCoreApplication::processEvents();
  QVERIFY(mesh->valid());QCOMPARE(mesh->localPositions().size(),6);QVERIFY(mesh->boundsMax().x()>mesh->boundsMin().x());QCOMPARE(camera->geometry(),mesh);
  QCOMPARE(camera->viewportSize(),QSizeF(viewport->width(),viewport->height()));
  camera->standardView(5);camera->fit();
  const QPointF center(viewport->width()/2,viewport->height()/2);
  const auto hit=camera->pick(center.x(),center.y());QCOMPARE(hit.value("bodyId").toString(),QString("near"));QCOMPARE(hit.value("triangleIndex").toInt(),1);QCOMPARE(hit.value("kind").toString(),QString("meshTriangle"));
  QVERIFY(std::abs(hit.value("position").toList()[2].toDouble()-(origin+1))<.001);
  QCOMPARE(native->property("camera").value<QObject*>(),perspective);
  QCOMPARE(perspective->property("position").value<QVector3D>(),camera->eye());
  QCOMPARE(perspective->property("rotation").value<QQuaternion>(),camera->orientation());
  const auto topForward=perspective->property("rotation").value<QQuaternion>().rotatedVector({0,0,-1});QVERIFY((topForward-QVector3D(0,0,-1)).length()<1e-5);
  camera->standardView(1);const auto frontRotation=perspective->property("rotation").value<QQuaternion>();QVERIFY((frontRotation.rotatedVector({0,0,-1})-QVector3D(0,1,0)).length()<1e-5);QVERIFY((frontRotation.rotatedVector({0,1,0})-QVector3D(0,0,1)).length()<1e-5);camera->standardView(5);

  const auto originalEye=camera->eye();camera->orbit(60,20);QVERIFY(camera->eye()!=originalEye);
  QCOMPARE(perspective->property("position").value<QVector3D>(),camera->eye());
  camera->standardView(5);camera->fit();
  const auto panPoint=camera->center();const auto beforePan=camera->project(panPoint);camera->pan(23,-17);
  const auto afterPan=camera->project(panPoint);QVERIFY((afterPan-beforePan-QPointF(23,-17)).manhattanLength()<.001);
  const auto beforeZoom=camera->distance();camera->zoomBy(.5);QVERIFY(camera->distance()<beforeZoom);
  QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("fitViewButton"),"clicked"));QVERIFY(camera->center().length()<.001);
  QVERIFY(QMetaObject::invokeMethod(root->findChild<QObject*>("projectionButton"),"clicked"));QVERIFY(!camera->perspective());QCOMPARE(native->property("camera").value<QObject*>(),orthographic);
  QVERIFY(std::abs(orthographic->property("horizontalMagnification").toDouble()-camera->magnification())<1e-5);
  QCOMPARE(camera->pick(center.x(),center.y()).value("bodyId").toString(),QString("near"));
  const QPoint click=viewport->mapToScene(center).toPoint();QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,click);QCOMPARE(workspace.selectedBody(),QString("near"));
  QCOMPARE(mesh->selectedBodyId(),QString("near"));QCOMPARE(mesh->selectedTriangleCount(),1);QCOMPARE(mesh->stride(),40);
  QVERIFY(root->findChild<QObject*>("meshMaterial")->property("vertexColorsEnabled").toBool());
  float unselectedRgba[4],selectedRgba[4];const auto colored=mesh->vertexData();std::memcpy(unselectedRgba,colored.constData()+24,16);std::memcpy(selectedRgba,colored.constData()+3*40+24,16);
  QVERIFY(std::abs(unselectedRgba[0]-selectedRgba[0])+std::abs(unselectedRgba[1]-selectedRgba[1])+std::abs(unselectedRgba[2]-selectedRgba[2])>.01);
  camera->pan(11,7);const auto selectionEye=camera->eye();const auto selectionTarget=camera->center();workspace.selectBody("far");
  QCOMPARE(mesh->selectedBodyId(),QString("far"));QCOMPARE(mesh->selectedTriangleCount(),1);QCOMPARE(camera->eye(),selectionEye);QCOMPARE(camera->center(),selectionTarget);
  const auto recolored=mesh->vertexData();QVERIFY(recolored!=colored);

  camera->standardView(5);camera->fit();const auto panOrigin=camera->center();
  QTest::mousePress(window,Qt::RightButton,Qt::NoModifier,click);QTest::mouseMove(window,click+QPoint(25,10));QTest::mouseRelease(window,Qt::RightButton,Qt::NoModifier,click+QPoint(25,10));QVERIFY(camera->center()!=panOrigin);
  const auto dragEye=camera->eye();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,click);QTest::mouseMove(window,click+QPoint(35,15));QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,click+QPoint(35,15));QVERIFY(camera->eye()!=dragEye);
  for(int view=0;view<7;++view) { QObject *button=nullptr;for(auto *item:root->findChild<QQuickItem*>("viewControls")->childItems())if(item->objectName()=="standardView"+QString::number(view))button=item;QVERIFY(button);QVERIFY(QMetaObject::invokeMethod(button,"clicked"));camera->fit();QVERIFY((camera->project(camera->center())-center).manhattanLength()<.01); }
  // Negative contracts: invalid input cannot leave stale GPU bytes or a pickable scene.
  auto bad=workspace.sceneParts;auto part=bad[0].toMap();part["indices"]=QVariantList{0,1,99};bad[0]=part;mesh->setParts(bad);QVERIFY(!mesh->valid());QVERIFY(mesh->vertexData().isEmpty());QVERIFY(camera->pick(center.x(),center.y()).isEmpty());
  mesh->setParts(workspace.sceneParts);QVERIFY(mesh->valid());
  part["indices"]=QVariantList{0,1,-1};bad[0]=part;mesh->setParts(bad);QVERIFY(!mesh->valid());
  part["indices"]=QVariantList{0,1,1.5};bad[0]=part;mesh->setParts(bad);QVERIFY(!mesh->valid());
  part["indices"]=QVariantList{0,1,2};part["vertices"]=QVariantList{0,0,0,1,0,0,0,std::numeric_limits<double>::infinity(),0};bad[0]=part;mesh->setParts(bad);QVERIFY(!mesh->valid());
  mesh->setParts(workspace.sceneParts);QVERIFY(mesh->valid());
 }
 void activeBodyDimensionRouting() {
  QTemporaryDir dir; QVERIFY(dir.isValid());
  precision::preferences::PreferencesStore prefs(dir.filePath("prefs.json"));
  precision::preferences::PersonalVocabularyStore vocab(dir.filePath("private-cache.json"));
  ModelRowWorkspace workspace;
  workspace.rows.clear();
  for (const QString &id : {QString("A"),QString("B"),QString("unsupported")})
   workspace.rows.append(QVariantMap{{"id",id},{"label",id},{"suppressed",false}});
  workspace.dimensions["A"]={{"editable",true},{"type","box"},{"first",10.},{"second",11.},{"third",12.}};
  workspace.dimensions["B"]={{"editable",true},{"type","box"},{"first",20.},{"second",21.},{"third",22.}};
  const auto originalA=workspace.dimensions["A"];
  UiText ui(&prefs,&vocab); QQmlEngine engine;
  auto context=engine.rootContext(); context->setContextProperty("preferences",&prefs); context->setContextProperty("vocabulary",&vocab); context->setContextProperty("workspace",&workspace); context->setContextProperty("uiText",&ui); context->setContextProperty("buildVersion","test"); context->setContextProperty("buildTime","unavailable");
  QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(MAIN_QML_PATH)));
  QVERIFY2(component.isReady(),qPrintable(component.errorString()));
  std::unique_ptr<QObject> root(component.create()); QVERIFY2(root!=nullptr,qPrintable(component.errorString()));
  auto *window=qobject_cast<QQuickWindow*>(root.get()); QVERIFY(window); window->show();
  auto *tree=root->findChild<QQuickItem*>("modelTree"); QVERIFY(tree);
  auto *viewport=root->findChild<QQuickItem*>("viewport"); QVERIFY(viewport);
  // Resolve actual QML ids without adding production-only test hooks or copying handlers.
  const auto qmlObject=[&](const QString &id) { QQmlExpression expression(qmlContext(root.get()),root.get(),id); return expression.evaluate().value<QObject*>(); };
  auto *edit=qmlObject("editDimensionsButton"); auto *dialog=qmlObject("dimensionsDialog");
  auto *first=qmlObject("dimA"); auto *second=qmlObject("dimB"); auto *third=qmlObject("dimC");
  QVERIFY(edit); QVERIFY(dialog); QVERIFY(first); QVERIFY(second); QVERIFY(third);
  const auto clickRow=[&](int index) {
   tree->setProperty("currentIndex",index); QMetaObject::invokeMethod(tree,"forceLayout"); QCoreApplication::processEvents();
   QObject *row=tree->property("currentItem").value<QObject*>();
   return row && QMetaObject::invokeMethod(row,"clicked");
  };
  QTRY_COMPARE(tree->property("count").toInt(),3);
  QVERIFY(clickRow(0)); QCOMPARE(workspace.selectedBody(),QString("A"));
  const auto meshA=viewport->property("vertices").toList(); QVERIFY(!meshA.isEmpty());
  QVERIFY(clickRow(1)); QCOMPARE(workspace.selectedBody(),QString("B"));
  QObject *selectedRow=tree->property("currentItem").value<QObject*>();QVERIFY(selectedRow);QVERIFY(selectedRow->property("highlighted").toBool());
  QQmlExpression selectedState(qmlContext(selectedRow),selectedRow,"Accessible.selected");QVERIFY(selectedState.evaluate().toBool());
  QQmlExpression selectedName(qmlContext(selectedRow),selectedRow,"Accessible.name");QVERIFY(selectedName.evaluate().toString().contains("Selected"));

  QCOMPARE(root->property("selectedLeft").toString(),QString("A"));
  QCOMPARE(root->property("selectedRight").toString(),QString("B"));
  QCOMPARE(viewport->property("vertices").toList(),workspace.meshVertices()); QVERIFY(viewport->property("vertices").toList()!=meshA);
  QVERIFY(edit->property("enabled").toBool()); QVERIFY(QMetaObject::invokeMethod(edit,"clicked"));
  QVERIFY(dialog->property("visible").toBool()); QCOMPARE(root->property("editTargetId").toString(),QString("B"));
  QCOMPARE(first->property("text").toString(),QString("20")); QCOMPARE(second->property("text").toString(),QString("21")); QCOMPARE(third->property("text").toString(),QString("22"));
  QVERIFY(first->setProperty("text","25")); QVERIFY(second->setProperty("text","26")); QVERIFY(third->setProperty("text","27"));
  // Selection can change while a modal edit is open through a controller update.
  // Acceptance must retain the target captured when the real dialog opened.
  workspace.selectBody("A"); QCOMPARE(root->property("editTargetId").toString(),QString("B"));
  QVERIFY(QMetaObject::invokeMethod(dialog,"accept"));
  QTRY_VERIFY(!dialog->property("visible").toBool());
  QCOMPARE(workspace.updateCount,1); QCOMPARE(workspace.updatedBody,QString("B")); QCOMPARE(workspace.updatedDimensions,QVariantList({25.,26.,27.}));
  QCOMPARE(workspace.dimensions["A"],originalA); QCOMPARE(root->property("editTargetId").toString(),QString());
  QVERIFY(clickRow(2)); QCOMPARE(workspace.selectedBody(),QString("unsupported"));
  QCOMPARE(root->property("selectedLeft").toString(),QString("A")); QVERIFY(!edit->property("enabled").toBool());
  QVERIFY(QMetaObject::invokeMethod(root.get(),"openDimensions")); QVERIFY(!dialog->property("visible").toBool());
  workspace.selectBody("B"); workspace.setBusy(true); QVERIFY(!edit->property("enabled").toBool());
  QVERIFY(QMetaObject::invokeMethod(root.get(),"openDimensions")); QVERIFY(!dialog->property("visible").toBool());
  workspace.setBusy(false); QVERIFY(edit->property("enabled").toBool()); QVERIFY(QMetaObject::invokeMethod(edit,"clicked"));
  QCOMPARE(first->property("text").toString(),QString("25"));
  workspace.setBusy(true); QVERIFY(QMetaObject::invokeMethod(dialog,"accept")); QCOMPARE(workspace.updateCount,1); QTRY_VERIFY(!dialog->property("visible").toBool());
  workspace.setBusy(false); QVERIFY(QMetaObject::invokeMethod(edit,"clicked")); QVERIFY(dialog->property("visible").toBool());
  workspace.newDocument(); QCoreApplication::processEvents();
  QCOMPARE(workspace.selectedBody(),QString()); QCOMPARE(root->property("selectedLeft").toString(),QString()); QCOMPARE(root->property("selectedRight").toString(),QString());
  QCOMPARE(root->property("editTargetId").toString(),QString()); QTRY_VERIFY(!dialog->property("visible").toBool()); QVERIFY(!edit->property("enabled").toBool()); QVERIFY(viewport->property("vertices").toList().isEmpty());
  QVERIFY(QMetaObject::invokeMethod(dialog,"accepted")); QCOMPARE(workspace.updateCount,1);
 }
 void provenanceRemainsVisibleAtMinimumSize() {
  QTemporaryDir dir;QVERIFY(dir.isValid());precision::preferences::PreferencesStore prefs(dir.filePath("prefs.json"));precision::preferences::PersonalVocabularyStore vocab(dir.filePath("vocab.json"));WorkspaceController workspace;UiText ui(&prefs,&vocab);QQmlEngine engine;
  const QString version="0.1.0-dev.91.f29406b",time="2026-09-07 17:49:39 Eastern Daylight Time";
  auto context=engine.rootContext();context->setContextProperty("preferences",&prefs);context->setContextProperty("vocabulary",&vocab);context->setContextProperty("workspace",&workspace);context->setContextProperty("uiText",&ui);context->setContextProperty("buildVersion",version);context->setContextProperty("buildTime",time);
  QQmlComponent component(&engine,QUrl::fromLocalFile(QStringLiteral(MAIN_QML_PATH)));QVERIFY2(component.isReady(),qPrintable(component.errorString()));std::unique_ptr<QObject> root(component.create());QVERIFY2(root!=nullptr,qPrintable(component.errorString()));
  auto *window=qobject_cast<QQuickWindow*>(root.get());QVERIFY(window);auto *footer=root->findChild<QQuickItem*>("provenanceBar");auto *text=root->findChild<QQuickItem*>("versionInfo");auto *split=root->findChild<QQuickItem*>("workspaceSplit");QVERIFY(footer);QVERIFY(text);QVERIFY(split);
  QVERIFY(prefs.setFontScale(1.5));window->resize(800,600);window->show();
  for(const auto &language:QStringList{"en","yue","both"})for(const auto &theme:QStringList{"light","dark"}) {
    QVERIFY(prefs.setLanguageMode(language));QVERIFY(prefs.setTheme(theme));QCoreApplication::processEvents();QTest::qWait(20);QCoreApplication::processEvents();
    const auto rect=text->mapRectToScene(text->boundingRect()),barRect=footer->mapRectToScene(footer->boundingRect()),workspaceRect=split->mapRectToScene(split->boundingRect());
    QVERIFY(text->isVisible());QVERIFY(rect.left()>=0);QVERIFY(rect.right()<=window->width()+.1);QVERIFY(rect.top()>=0);QVERIFY(rect.bottom()<=window->height()+.1);
    QVERIFY(rect.height()>=text->property("contentHeight").toDouble()-.1);QVERIFY(rect.width()>=text->property("contentWidth").toDouble()-.1);
    QVERIFY(workspaceRect.bottom()<=barRect.top()+.1);QVERIFY(barRect.bottom()<=window->height()+.1);
    const auto value=text->property("text").toString();QVERIFY(value.contains(version));QVERIFY(value.contains(time));QQmlExpression accessible(qmlContext(text),text,"Accessible.name");QCOMPARE(accessible.evaluate().toString(),value);
  }
 }
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
  QVERIFY(prefs.setTheme("light")); QCoreApplication::processEvents(); QCOMPARE(root->property("effectiveTheme").toInt(),0); auto environment=root->findChild<QObject*>("sceneEnvironment"); QVERIFY(environment); const auto lightBackground=environment->property("clearColor");
  QVERIFY(prefs.setTheme("dark")); QCoreApplication::processEvents(); QCOMPARE(root->property("effectiveTheme").toInt(),1); QVERIFY(environment->property("clearColor")!=lightBackground);
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
  QVERIFY(prefs.setLanguageMode("en")); QCoreApplication::processEvents();
  const auto toolbarItems=flow->childItems();
  const QStringList toolbarActions{"Box","New","Cylinder","Union","Cut","Intersect","Undo","Redo","Fit","Save","Open","Settings"};
  QCOMPARE(toolbarItems.size(),toolbarActions.size()+1);
  QVERIFY2(toolbarItems.first()->inherits("QQuickLabel"),"the first toolbar item must remain the title label");
  for (qsizetype index=0;index<toolbarActions.size();++index) {
   auto *action=toolbarItems.at(index+1);
   QCOMPARE(action->property("text").toString(),toolbarActions.at(index));
   QVERIFY2(action->inherits("QQuickToolButton"),qPrintable(QStringLiteral("toolbar action %1 must use the native QQuickToolButton primitive, actual %2").arg(toolbarActions.at(index),QString::fromLatin1(action->metaObject()->className()))));
  }
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
  QVERIFY2(root->findChild<QQuickItem*>("provenanceBar")->mapRectToScene(root->findChild<QQuickItem*>("provenanceBar")->boundingRect()).contains(version->mapRectToScene(version->boundingRect())), "version provenance must be initially visible at the default client area");
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
