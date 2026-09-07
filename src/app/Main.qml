import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick3D as Quick3D
import PrecisionCad

ApplicationWindow {
  id: root
  width: 1280; height: 820; minimumWidth: 800; minimumHeight: 600; visible: true
  title: root.copy("Precision CAD","精準 CAD") + " · " + buildVersion
  Material.theme: preferences.theme === "dark" ? Material.Dark : preferences.theme === "light" ? Material.Light : Material.System
  Material.accent: preferences.accentColor
  font.pixelSize: 14 * preferences.fontScale
  readonly property int effectiveTheme: Material.theme
  property bool allowClose: false
  property bool saveForDeferred: false
  property string vocabularyStatus: ""
  property string selectedLeft: ""; property string selectedRight: ""
  property string dimensionType: ""
  property string editTargetId: ""
  property string deferredAction: ""
  property url documentFolder: typeof initialDocumentFolder === "undefined" ? "" : initialDocumentFolder
  function copy(en, yue) {
    var english = en
    var cantonese = yue
    if (en === "Pick two tree bodies for boolean operations.") {
      english = ["Select two bodies for a boolean operation.", "Pick two bodies, then choose the operation.", "Two bodies make a boolean operation possible.", "Pick a pair of bodies and let geometry do the arithmetic.", "Pick two bodies. Geometry will handle their merger negotiations."][preferences.englishTone - 1]
      cantonese = ["選取兩個實體以進行布林運算。", "揀兩個實體，再選擇運算。", "兩個實體齊備，就可以做布林運算。", "揀好兩個實體，幾何計數交畀運算。", "揀兩個實體，合併談判交畀幾何運算處理。"][preferences.cantoneseTone - 1]
    }
    var rendered = preferences.languageMode === "yue" ? cantonese : preferences.languageMode === "both" ? english + "\n" + cantonese : english
    return vocabulary.loaded ? uiText.apply(rendered) : rendered
  }
  function guard(action) { if(workspace.busy) return; if (workspace.dirty) { deferredAction = action; unsavedDialog.open() } else perform(action) }
  function perform(action) { if(action === "new") workspace.newDocument(); else if(action === "open") openDialog.open(); else if(action === "close") { allowClose=true; root.close() } }
  function openDimensions() { var target=workspace.selectedBody; var dimensions=workspace.editableDimensions(target); if (!dimensions.editable || workspace.busy) return; editTargetId=target; dimensionType=dimensions.type; dimA.text=dimensions.first; dimB.text=dimensions.second; dimC.text=dimensions.type === "box" ? dimensions.third : ""; dimensionsDialog.open() }
  onClosing: (close)=> { if(!allowClose && (workspace.dirty || workspace.busy)) { close.accepted=false; guard("close") } }
  header: ToolBar {
    objectName: "mainToolbar"
    implicitHeight: toolbarFlow.implicitHeight + topPadding + bottomPadding
    Flow { id: toolbarFlow; objectName: "toolbarFlow"; width: parent.width; padding: 8; spacing: 8
      Label { textFormat: Text.PlainText; text: root.copy("Precision CAD", "精準 CAD"); font.pixelSize: 20; font.bold: true; width: 230 }
      ToolButton { text: root.copy("Box", "方盒"); onClicked: boxDialog.open() }
      ToolButton { text: root.copy("New", "新檔"); onClicked: root.guard("new") }
      ToolButton { text: root.copy("Cylinder", "圓柱"); onClicked: cylinderDialog.open() }
      ToolButton { text: root.copy("Union","合併"); enabled: root.selectedLeft !== "" && root.selectedRight !== ""; onClicked: workspace.booleanOperation("union", root.selectedLeft, root.selectedRight) }
      ToolButton { text: root.copy("Cut","切除"); enabled: root.selectedLeft !== "" && root.selectedRight !== ""; onClicked: workspace.booleanOperation("cut", root.selectedLeft, root.selectedRight) }
      ToolButton { text: root.copy("Intersect","相交"); enabled: root.selectedLeft !== "" && root.selectedRight !== ""; onClicked: workspace.booleanOperation("intersection", root.selectedLeft, root.selectedRight) }

      ToolButton { text: root.copy("Undo", "復原"); onClicked: workspace.undo() }
      ToolButton { text: root.copy("Redo", "重做"); onClicked: workspace.redo() }
      ToolButton { objectName: "fitViewButton"; text: root.copy("Fit", "置中"); onClicked: viewport.fit() }
      ToolButton { text: root.copy("Save", "儲存"); onClicked: { root.deferredAction=""; root.saveForDeferred=false; saveDialog.open() } }
      ToolButton { text: root.copy("Open", "開啟"); onClicked: root.guard("open") }
      ToolButton { text: root.copy("Settings", "設定"); onClicked: settings.open() }
    }
  }
  FileDialog { id: saveDialog; currentFolder: root.documentFolder; objectName: "saveDialog"; title: root.copy("Save native Precision CAD document","儲存原生精準 CAD 文件"); fileMode: FileDialog.SaveFile; nameFilters: [root.copy("Precision CAD documents","精準 CAD 文件") + " (*.pcad)"]; onAccepted: workspace.save(workspace.localPath(selectedFile)); onRejected: { root.deferredAction=""; root.saveForDeferred=false } }
  FileDialog { id: openDialog; currentFolder: root.documentFolder; title: root.copy("Open native Precision CAD document","開啟原生精準 CAD 文件"); fileMode: FileDialog.OpenFile; nameFilters: [root.copy("Precision CAD documents","精準 CAD 文件") + " (*.pcad)"]; onAccepted: workspace.open(workspace.localPath(selectedFile)) }
  FileDialog { id: vocabularyDialog; currentFolder: root.documentFolder; title: root.copy("Load local vocabulary","載入本機詞彙"); fileMode: FileDialog.OpenFile; nameFilters: [root.copy("Vocabulary JSON","詞彙 JSON") + " (*.json)"]; onAccepted: { root.vocabularyStatus = uiText.loadVocabulary(selectedFile) ? root.copy("Local vocabulary loaded.","已載入本機詞彙。") : root.copy("Local vocabulary was not loaded.","未能載入本機詞彙。") } }
  Connections { target: workspace
    function onSaveFinished(ok, message) { var next=root.saveForDeferred ? root.deferredAction : ""; root.deferredAction=""; root.saveForDeferred=false; if(ok && next!=="") root.perform(next) }
    function onDocumentChanged() { root.selectedLeft=""; root.selectedRight=""; if (dimensionsDialog.visible) dimensionsDialog.close(); root.editTargetId="" }
  }
  Dialog { id: unsavedDialog; objectName: "unsavedDialog"; onRejected: { root.deferredAction=""; root.saveForDeferred=false } title: (preferences.dialogEmojis ? "⚠ " : "") + root.copy("Unsaved changes","未儲存的變更"); modal: true; standardButtons: Dialog.NoButton
    Column { padding: 20; spacing: 12
      Label { textFormat: Text.PlainText; text: root.copy("Save changes before continuing?","繼續之前要儲存變更嗎？") }
      Row { spacing: 8
      Button { text: root.copy("Save","儲存"); onClicked: { root.saveForDeferred=true; saveDialog.open(); unsavedDialog.close() } }
      Button { text: root.copy("Discard","捨棄"); onClicked: { var a=root.deferredAction; root.deferredAction=""; unsavedDialog.close(); root.perform(a) } }
      Button { text: root.copy("Cancel","取消"); onClicked: { root.deferredAction=""; root.saveForDeferred=false; unsavedDialog.close() } }
    } }
  }
  Drawer { id: settings; enter: Transition { NumberAnimation { property: "position"; duration: preferences.reducedMotion ? 0 : 180 } } exit: Transition { NumberAnimation { property: "position"; duration: preferences.reducedMotion ? 0 : 180 } } edge: Qt.RightEdge; width: Math.min(390, root.width*.9); height: root.height
    ScrollView { anchors.fill: parent; contentWidth: availableWidth
    Column { width: parent.width; padding: 18; spacing: 12
      Label { textFormat: Text.PlainText; text: root.copy("Preferences","偏好設定"); font.pixelSize: 22; font.bold: true }
      ComboBox { objectName: "languageMode"; model: [root.copy("English","英文"),root.copy("Cantonese","廣東話"),root.copy("Bilingual","雙語")]; currentIndex: ["en","yue","both"].indexOf(preferences.languageMode); onActivated: preferences.setLanguageMode(["en","yue","both"][currentIndex]) }
       Label { textFormat: Text.PlainText; text: root.copy("English tone","英文語氣") }
       Slider { from: 1; to: 5; stepSize: 1; value: preferences.englishTone; onMoved: preferences.setEnglishTone(Math.round(value)) }
       Label { textFormat: Text.PlainText; text: root.copy("Cantonese tone","廣東話語氣") }
       Slider { from: 1; to: 5; stepSize: 1; value: preferences.cantoneseTone; onMoved: preferences.setCantoneseTone(Math.round(value)) }
      Switch { text: root.copy("Dialog emoji","對話框表情符號"); checked: preferences.dialogEmojis; onToggled: preferences.setDialogEmojis(checked) }
      ComboBox { model:[root.copy("Dark","深色"),root.copy("Light","淺色"),root.copy("System","跟隨作業系統")]; currentIndex:["dark","light","system"].indexOf(preferences.theme); onActivated: preferences.setTheme(["dark","light","system"][currentIndex]) }
       Label { textFormat: Text.PlainText; text: root.copy("Font scale","字體比例") }
       Slider { from:.8; to:1.5; value:preferences.fontScale; onMoved: preferences.setFontScale(value) }
      Switch { text: root.copy("Reduced motion","減少動態效果"); checked:preferences.reducedMotion; onToggled:preferences.setReducedMotion(checked) }
      Switch { text: root.copy("ADHD mode","專注模式"); checked:preferences.adhdMode; onToggled:preferences.setAdhdMode(checked) }
      Label { width: parent.width - 36; textFormat: Text.PlainText; text: vocabulary.loaded ? root.copy("Local vocabulary is loaded for this private session.","已為本機私人使用載入詞彙。") : root.copy("Local vocabulary is not loaded.","尚未載入本機詞彙。"); wrapMode: Text.WordWrap }
      Button { text: vocabulary.loaded ? root.copy("Replace local vocabulary","更換本機詞彙") : root.copy("Load local vocabulary","載入本機詞彙"); onClicked: vocabularyDialog.open() }
      Label { width: parent.width - 36; textFormat: Text.PlainText; text: root.vocabularyStatus; wrapMode: Text.WordWrap }
      Button { text: root.copy("Clear local vocabulary","清除本機詞彙"); enabled: vocabulary.loaded; onClicked: { if(vocabulary.clear()) root.vocabularyStatus=root.copy("Local vocabulary cleared.","已清除本機詞彙。") } }
      Label { width: parent.width - 36; textFormat: Text.PlainText; text: root.copy("Narration is stored but unavailable until a local voice runtime is integrated.","已儲存朗讀設定，但本機語音功能尚未整合。"); wrapMode: Text.WordWrap }
    }
  }
  }
  Dialog { id: boxDialog; title: root.copy("Dimensioned box","指定尺寸的方盒"); modal: true; standardButtons: Dialog.NoButton
    footer: DialogButtonBox { Button { text: root.copy("OK","確定"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole } Button { text: root.copy("Cancel","取消"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole } }
    onAccepted: workspace.addBox(Number(dx.text), Number(dy.text), Number(dz.text))
    Grid { columns: 2; spacing: 12; padding: 16
      Label { textFormat: Text.PlainText; text: root.copy("X (mm)","X（毫米）") }
      TextField { id: dx; text: "40"; validator: DoubleValidator { bottom: 0.001 } }
      Label { textFormat: Text.PlainText; text: root.copy("Y (mm)","Y（毫米）") }
      TextField { id: dy; text: "30"; validator: DoubleValidator { bottom: 0.001 } }
      Label { textFormat: Text.PlainText; text: root.copy("Z (mm)","Z（毫米）") }
      TextField { id: dz; text: "20"; validator: DoubleValidator { bottom: 0.001 } }
    }
  }
  Dialog { id: cylinderDialog; title: root.copy("Dimensioned cylinder","指定尺寸的圓柱"); modal: true; standardButtons: Dialog.NoButton
    footer: DialogButtonBox { Button { text: root.copy("OK","確定"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole } Button { text: root.copy("Cancel","取消"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole } }
    onAccepted: workspace.addCylinder(Number(radius.text), Number(height.text))
    Grid { columns: 2; spacing: 12; padding: 16
      Label { textFormat: Text.PlainText; text: root.copy("Radius (mm)","半徑（毫米）") }
      TextField { id: radius; text: "12"; validator: DoubleValidator { bottom: 0.001 } }
      Label { textFormat: Text.PlainText; text: root.copy("Height (mm)","高度（毫米）") }
      TextField { id: height; text: "45"; validator: DoubleValidator { bottom: 0.001 } }
    }
  }
  SplitView { objectName: "workspaceSplit"; anchors.fill: parent
    Pane { objectName: "modelPane"; SplitView.preferredWidth: 310
      Column { objectName: "modelColumn"; anchors.fill: parent; spacing: 8
        Label { id: modelHeader; textFormat: Text.PlainText; text: root.copy("Model tree","模型樹"); font.bold: true; font.pixelSize: 18 }
        ListView { id: tree; objectName: "modelTree"; width: parent.width; height: Math.max(0, parent.height - modelHeader.implicitHeight - editDimensionsButton.implicitHeight - selectionHelp.implicitHeight - parent.spacing * 3); model: workspace.features; clip: true
          delegate: ItemDelegate { width: tree.width; highlighted: workspace.selectedBody === modelData.id
            Accessible.role: Accessible.ListItem
            Accessible.selected: workspace.selectedBody === modelData.id
            Accessible.name: modelData.label + (workspace.selectedBody === modelData.id ? " " + root.copy("Selected", "已選取") : "")
            text: (modelData.suppressed ? "⊘ " : "") + modelData.label + "  " + modelData.id.slice(0, 8)
            onClicked: { if(root.selectedLeft === modelData.id) root.selectedLeft = ""; else if(root.selectedRight === modelData.id) root.selectedRight = ""; else if(root.selectedLeft === "") root.selectedLeft = modelData.id; else root.selectedRight = modelData.id; workspace.selectBody(modelData.id) }
            contentItem: Row { id: modelRow; objectName: "modelRow"; width: parent.width; spacing: 8
              Label { id: modelRowLabel; objectName: "modelRowLabel"; textFormat: Text.PlainText; text: (workspace.selectedBody === modelData.id ? root.copy("Selected", "已選取") + " · " : "") + (modelData.suppressed ? "⊘ " : "") + modelData.label + "  " + modelData.id.slice(0,8); anchors.verticalCenter: parent.verticalCenter; width: Math.max(0, modelRow.width - includeBody.implicitWidth - modelRow.spacing); elide: Text.ElideRight }
              Switch { id: includeBody; objectName: "modelRowIncludeSwitch"; Accessible.name: root.copy("Include body","啟用實體"); checked: !modelData.suppressed; onToggled: workspace.suppressFeature(modelData.id, !checked) }
            }
          }
        }
        Button { id: editDimensionsButton; text: root.copy("Edit selected dimensions","編輯所選尺寸"); enabled: !workspace.busy && workspace.editableDimensions(workspace.selectedBody).editable; onClicked: root.openDimensions() }
        Label { id: selectionHelp; objectName: "selectionHelp"; width: parent.width; textFormat: Text.PlainText; text: root.copy("Pick two tree bodies for boolean operations.","揀兩個實體進行布林運算。"); wrapMode: Text.WordWrap; opacity: preferences.adhdMode ? 1 : .8 }
      }
    }
    Pane { SplitView.fillWidth: true
      Item { id: viewport; objectName: "viewport"; anchors.fill: parent
        property var vertices: workspace.meshVertices
        property var indices: workspace.meshIndices
        property alias cameraController: cameraState
        function fit() { cameraState.fit() }
        ViewportCamera { id: cameraState; objectName: "cameraController"; viewportSize: Qt.size(viewport.width, viewport.height); geometry: sceneMesh }
        Quick3D.View3D { id: nativeView; objectName: "nativeView"; anchors.fill: parent
          renderMode: Quick3D.View3D.Offscreen
          environment: Quick3D.SceneEnvironment { objectName: "sceneEnvironment"; clearColor: root.Material.background; backgroundMode: Quick3D.SceneEnvironment.Color; depthTestEnabled: true; antialiasingMode: Quick3D.SceneEnvironment.MSAA; antialiasingQuality: Quick3D.SceneEnvironment.High }
          camera: cameraState.perspective ? perspectiveCamera : orthographicCamera
          Quick3D.PerspectiveCamera { id: perspectiveCamera; objectName: "perspectiveCamera"; position: cameraState.position; rotation: cameraState.orientation; fieldOfView: cameraState.fieldOfView; fieldOfViewOrientation: Quick3D.PerspectiveCamera.Vertical; clipNear: cameraState.clipNear; clipFar: cameraState.clipFar }
          Quick3D.OrthographicCamera { id: orthographicCamera; objectName: "orthographicCamera"; position: cameraState.position; rotation: cameraState.orientation; horizontalMagnification: cameraState.magnification; verticalMagnification: cameraState.magnification; clipNear: cameraState.clipNear; clipFar: cameraState.clipFar }
          Quick3D.DirectionalLight { eulerRotation: Qt.vector3d(-35, -45, 0); brightness: 1.25 }
          Quick3D.DirectionalLight { eulerRotation: Qt.vector3d(35, 135, 0); brightness: .45 }
          Quick3D.Model { id: meshModel; objectName: "meshModel"; visible: sceneMesh.valid; pickable: true
            geometry: MeshGeometry { id: sceneMesh; objectName: "sceneMesh"; vertices: viewport.vertices; indices: viewport.indices; normals: typeof workspace.meshNormals === "undefined" ? [] : workspace.meshNormals; parts: typeof workspace.meshParts === "undefined" ? [] : workspace.meshParts; fallbackBodyId: workspace.selectedBody; selectedBodyId: workspace.selectedBody; baseColor: root.Material.foreground; selectionColor: root.Material.accent }
            materials: Quick3D.PrincipledMaterial { objectName: "meshMaterial"; baseColor: "white"; vertexColorsEnabled: true; roughness: .65; metalness: .15; cullMode: Quick3D.Material.NoCulling }
          }
        }
        Label { anchors.centerIn: parent; visible: !sceneMesh.valid; textFormat: Text.PlainText; text: root.copy("No valid regenerated mesh","尚未生成有效網格"); color: Material.foreground }
        MouseArea { id: viewportInput; objectName: "viewportInput"; anchors.fill: parent
          property real lastX; property real lastY; property real pressX; property real pressY; property bool dragged: false
          acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
          onPressed: (m)=> { lastX=pressX=m.x; lastY=pressY=m.y; dragged=false }
          onPositionChanged: (m)=> {
            if (Math.abs(m.x-pressX)+Math.abs(m.y-pressY)>4) dragged=true
            if(pressedButtons & (Qt.RightButton | Qt.MiddleButton) || ((pressedButtons & Qt.LeftButton) && (m.modifiers & Qt.ShiftModifier))) cameraState.pan(m.x-lastX,m.y-lastY)
            else if(pressedButtons & Qt.LeftButton) cameraState.orbit(m.x-lastX,m.y-lastY)
            lastX=m.x; lastY=m.y
          }
          onReleased: (m)=> { if(!dragged && m.button===Qt.LeftButton) { var hit=cameraState.pick(m.x,m.y); if(hit.bodyId)workspace.selectBody(hit.bodyId) } }
          onDoubleClicked: viewport.fit()
          onWheel: (w)=> { cameraState.zoomBy(Math.exp(-w.angleDelta.y*.001)); w.accepted=true }
        }
        Flow { objectName: "viewControls"; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; spacing: 4
      Label { text: root.copy("Z up · XY top", "Z 向上 · XY 頂視"); Accessible.name: root.copy("CAD axes: Z up; top view is the XY plane", "CAD 座標：Z 向上，頂視為 XY 平面"); padding: 8 }
      ToolButton { objectName: "projectionButton"; text: cameraState.perspective ? root.copy("Orthographic", "正投影") : root.copy("Perspective", "透視"); onClicked: cameraState.perspective=!cameraState.perspective }
      Repeater { model: [root.copy("Iso","等角"),root.copy("Front","前"),root.copy("Back","後"),root.copy("Left","左"),root.copy("Right","右"),root.copy("Top","頂"),root.copy("Bottom","底")]
        ToolButton { required property int index; required property string modelData; objectName: "standardView"+index; text: modelData; onClicked: cameraState.standardView(index) }
      }
        }
      }
    }

    Pane { objectName: "inspectorPane"; SplitView.preferredWidth: 290
      ScrollView { id: inspectorScroll; objectName: "inspectorScroll"; anchors.fill: parent; contentWidth: availableWidth
      Column { objectName: "inspectorColumn"; width: inspectorScroll.availableWidth; spacing: 10
        Label { width: parent.width; textFormat: Text.PlainText; text: root.copy("Operation","運算"); font.bold: true; font.pixelSize: 18; wrapMode: Text.WordWrap }
        Label { width: parent.width; textFormat: Text.PlainText; text: workspace.busy ? root.copy("Regenerating","重新生成中") : workspace.operationState === "Ready" ? root.copy("Ready","就緒") : workspace.operationState === "Cancelled" ? root.copy("Cancelled","已取消") : root.copy("Failed","未能完成"); wrapMode: Text.WordWrap }
        Label { objectName: "rawDiagnostics"; width: parent.width; textFormat: Text.PlainText; visible: workspace.errorMessage.length > 0; text: workspace.errorMessage; color: Material.color(Material.Red); wrapMode: Text.WordWrap }
        Button { text: root.copy("Cancel geometry","取消幾何運算"); enabled: workspace.busy; onClicked: workspace.cancel() }
        Label { width: parent.width; textFormat: Text.PlainText; text: root.copy("Measurements","量度"); font.bold: true; font.pixelSize: 18; wrapMode: Text.WordWrap }
        Label { width: parent.width; textFormat: Text.PlainText; text: root.copy("Volume:","體積：") + " " + workspace.volume; wrapMode: Text.WordWrap }
        Label { width: parent.width; textFormat: Text.PlainText; text: root.copy("Bounds:","邊界：") + " " + workspace.bounds; wrapMode: Text.WordWrap }
        Rectangle { width: parent.width; height: 1; color: "#555b66" }
        Label { objectName: "versionInfo"; width: parent.width; textFormat: Text.PlainText; text: root.copy("Version","版本") + " " + buildVersion + "\n" + root.copy("Updated","更新時間") + " " + buildTime; wrapMode: Text.WordWrap }
        Label { objectName: "inspectorNotice"; width: parent.width; textFormat: Text.PlainText; text: root.copy("CAM, FEA, advanced editing, and the complete settings and history suite are unfinished in this modelling slice.","此建模階段尚未完成 CAM、FEA、圓角編輯及完整歷史功能。"); wrapMode: Text.WordWrap; opacity: preferences.adhdMode ? 1 : .8 }
      }
      }
    }
  }
  Dialog { id: dimensionsDialog; title: root.copy("Edit dimensions","編輯尺寸"); modal: true; standardButtons: Dialog.NoButton
    footer: DialogButtonBox { Button { text: root.copy("OK","確定"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole } Button { text: root.copy("Cancel","取消"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole } }
    onAccepted: { if (root.editTargetId !== "" && !workspace.busy) workspace.updateDimensions(root.editTargetId, Number(dimA.text), Number(dimB.text), root.dimensionType === "box" ? Number(dimC.text) : 1); root.editTargetId="" }
    onRejected: root.editTargetId=""
    Grid { columns:2; padding:16; spacing:8
      Label { textFormat: Text.PlainText; text: root.dimensionType === "cylinder" ? root.copy("Radius (mm)","半徑（毫米）") : root.copy("X (mm)","X（毫米）") }
      TextField { id:dimA; validator: DoubleValidator { bottom: 0.001 } }
      Label { textFormat: Text.PlainText; text: root.dimensionType === "cylinder" ? root.copy("Height (mm)","高度（毫米）") : root.copy("Y (mm)","Y（毫米）") }
      TextField { id:dimB; validator: DoubleValidator { bottom: 0.001 } }
      Label { visible: root.dimensionType === "box"; textFormat: Text.PlainText; text: root.copy("Z (mm)","Z（毫米）") }
      TextField { id:dimC; visible: root.dimensionType === "box"; validator: DoubleValidator { bottom: 0.001 } }
    }
  }
}
