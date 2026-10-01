#include <QtTest>

// This test exercises the public, non-QML math seams for the native viewport.
// Rendering and input event plumbing stay outside this deterministic contract.
#include "viewport/viewport_camera.h"
#include "viewport/viewport_picker.h"

namespace {

constexpr qreal kTolerance = 0.0001;

void verifyVectorNear(const QVector3D &actual, const QVector3D &expected) {
  QVERIFY2((actual - expected).length() <= kTolerance,
           qPrintable(QStringLiteral("actual=(%1, %2, %3), expected=(%4, %5, %6)")
                         .arg(actual.x()).arg(actual.y()).arg(actual.z())
                         .arg(expected.x()).arg(expected.y()).arg(expected.z())));
}

} // namespace

class ViewportMathTest final : public QObject {
  Q_OBJECT

private slots:
  void fittedSceneSurvivesPortraitResize() {
    precision::app::viewport::ViewportCamera camera;
    camera.setViewport({800,600}); camera.setSceneBounds({-5,-5,-5},{5,5,5});
    camera.setStandardView(precision::app::viewport::StandardView::Front);
    camera.setViewport({50,600});
    const auto edge=camera.project({5,0,5});
    QVERIFY(edge.x()>=0 && edge.x()<=50);
    camera.pan(10,15);const auto target=camera.center();camera.setViewport({600,800});
    QCOMPARE(camera.center(),target);
  }
  void frontViewCenterRayFacesSceneCenter() {
    precision::app::viewport::ViewportCamera camera;
    camera.setViewport({800.0, 600.0});
    camera.setSceneBounds({-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f});
    camera.setProjection(precision::app::viewport::Projection::Perspective);
    camera.setStandardView(precision::app::viewport::StandardView::Front);

    const auto ray = camera.rayForScreenPoint(QPointF(400.0, 300.0));
    QVERIFY(ray.origin.y() < -5.0f);
    verifyVectorNear(ray.direction, QVector3D(0.0f, 1.0f, 0.0f));
  }

  void cadViewsUseZUpAndExactPlanProjection() {
    precision::app::viewport::ViewportCamera camera;camera.setViewport({800,600});camera.setSceneBounds({-5,-5,-5},{5,5,5});camera.setProjection(precision::app::viewport::Projection::Orthographic);
    camera.standardView(5);verifyVectorNear(camera.rayForScreenPoint({400,300}).direction,{0,0,-1});verifyVectorNear(camera.up(),{0,1,0});
    QVERIFY(camera.project({1,0,0}).x()>400);QVERIFY(camera.project({0,1,0}).y()<300);QVERIFY((camera.project({0,0,1})-QPointF(400,300)).manhattanLength()<.001);
    camera.standardView(1);verifyVectorNear(camera.rayForScreenPoint({400,300}).direction,{0,1,0});verifyVectorNear(camera.up(),{0,0,1});QVERIFY(camera.project({0,0,1}).y()<300);
    camera.standardView(4);verifyVectorNear(camera.rayForScreenPoint({400,300}).direction,{-1,0,0});verifyVectorNear(camera.up(),{0,0,1});
    camera.standardView(0);QVERIFY(camera.eye().x()>0);QVERIFY(camera.eye().y()<0);QVERIFY(camera.eye().z()>0);
    verifyVectorNear(camera.orientation().rotatedVector({0,0,-1}),camera.rayForScreenPoint({400,300}).direction);
  }
  void standardViewsAndOrbitKeepSceneCenterAtViewportCenter() {
    precision::app::viewport::ViewportCamera camera;
    camera.setViewport({800.0, 600.0});
    camera.setSceneBounds({-5.0f, -5.0f, -5.0f}, {5.0f, 5.0f, 5.0f});
    camera.setProjection(precision::app::viewport::Projection::Orthographic);
    camera.setStandardView(precision::app::viewport::StandardView::Isometric);

    const QPointF isometricCenter = camera.project({0.0f, 0.0f, 0.0f});
    QCOMPARE(qAbs(isometricCenter.x() - 400.0) <= kTolerance, true);
    QCOMPARE(qAbs(isometricCenter.y() - 300.0) <= kTolerance, true);

    camera.setOrbitDegrees(35.0, -20.0);
    const QPointF orbitCenter = camera.project({0.0f, 0.0f, 0.0f});
    QCOMPARE(qAbs(orbitCenter.x() - 400.0) <= kTolerance, true);
    QCOMPARE(qAbs(orbitCenter.y() - 300.0) <= kTolerance, true);
  }

  void rayPickingReturnsNearestFaceHit() {
    const precision::app::viewport::Ray ray{
      .origin = {0.0f, 0.0f, 10.0f},
      .direction = {0.0f, 0.0f, -1.0f},
    };
    precision::app::viewport::MeshData mesh{
      .vertices = {
        {-2.0f, -2.0f, 0.0f}, {2.0f, -2.0f, 0.0f}, {0.0f, 2.0f, 0.0f},
        {-2.0f, -2.0f, 5.0f}, {2.0f, -2.0f, 5.0f}, {0.0f, 2.0f, 5.0f},
      },
      .indices = {0, 1, 2, 3, 4, 5},
    };

    const auto hit = precision::app::viewport::ViewportPicker::pick(mesh, ray, 0.01f);
    QCOMPARE(hit.kind, precision::app::viewport::PickKind::Face);
    QCOMPARE(hit.triangleIndex, 1);
    QCOMPARE(qAbs(hit.distance - 5.0f) <= kTolerance, true);
  }

  void rayPickingPrefersMeshVertexThenVisualEdge() {
    const precision::app::viewport::MeshData mesh{
      .vertices = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      .indices = {0, 1, 2},
    };
    const precision::app::viewport::Ray vertexRay{
      .origin = {-1.0f, -1.0f, 10.0f},
      .direction = {0.0f, 0.0f, -1.0f},
    };
    const auto vertexHit = precision::app::viewport::ViewportPicker::pick(mesh, vertexRay, 0.05f);
    QCOMPARE(vertexHit.kind, precision::app::viewport::PickKind::Vertex);
    QCOMPARE(vertexHit.triangleIndex, 0);
    QCOMPARE(vertexHit.vertexIndex, 0);

    const precision::app::viewport::Ray edgeRay{
      .origin = {0.0f, -1.0f, 10.0f},
      .direction = {0.0f, 0.0f, -1.0f},
    };
    const auto edgeHit = precision::app::viewport::ViewportPicker::pick(mesh, edgeRay, 0.05f);
    QCOMPARE(edgeHit.kind, precision::app::viewport::PickKind::Edge);
    QCOMPARE(edgeHit.triangleIndex, 0);
  }

  void rayPickingRejectsMissesBackwardsHitsAndInvalidIndices() {
    const precision::app::viewport::Ray ray{
      .origin = {0.0f, 0.0f, 0.0f},
      .direction = {0.0f, 0.0f, -1.0f},
    };
    precision::app::viewport::MeshData mesh{
      .vertices = {
        {4.0f, 4.0f, -5.0f}, {6.0f, 4.0f, -5.0f}, {5.0f, 6.0f, -5.0f},
        {-2.0f, -2.0f, 5.0f}, {2.0f, -2.0f, 5.0f}, {0.0f, 2.0f, 5.0f},
      },
      .indices = {0, 1, 2},
    };

    QCOMPARE(precision::app::viewport::ViewportPicker::pick(mesh, ray, 0.01f).kind,
             precision::app::viewport::PickKind::None);
    mesh.indices = {3, 4, 5};
    QCOMPARE(precision::app::viewport::ViewportPicker::pick(mesh, ray, 0.01f).kind,
             precision::app::viewport::PickKind::None);
    mesh.indices = {0, 1, 99};
    QCOMPARE(precision::app::viewport::ViewportPicker::pick(mesh, ray, 0.01f).kind,
             precision::app::viewport::PickKind::None);
    mesh.indices = {0, 1};
    QCOMPARE(precision::app::viewport::ViewportPicker::pick(mesh, ray, 0.01f).kind,
             precision::app::viewport::PickKind::None);
  }
};

QTEST_APPLESS_MAIN(ViewportMathTest)

#include "tst_viewport_math.moc"
