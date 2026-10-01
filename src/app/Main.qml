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
  property string sketchEditId: ""
  property string padSketchId: ""
  property string padRegionId: ""
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
  function openDimensions() { var target=workspace.selectedBody; var dimensions=workspace.editableDimensions(target); if (!dimensions.editable || workspace.busy) return; editTargetId=target; dimensionType=dimensions.type; dimA.text=dimensions.first; dimB.text=dimensions.type === "pad" ? "" : dimensions.second; dimC.text=dimensions.type === "box" ? dimensions.third : ""; dimensionsDialog.open() }
  function openSketchEditor(id) {
    if (workspace.busy) return
    var editable = id === "" ? ({ editable: true, plane: "xy", width: 60, height: 40, holeRadius: 8, holeU: 30, holeV: 20 }) : workspace.editableSketch(id)
    if (!editable.editable) return
    sketchEditId = id
    sketchPlane.currentIndex = Math.max(0, ["xy", "yz", "zx"].indexOf(editable.plane))
    sketchWidth.text = editable.width
    sketchHeight.text = editable.height
    sketchHoleRadius.text = editable.holeRadius
    sketchHoleU.text = editable.holeU
    sketchHoleV.text = editable.holeV
    sketchEditor.open()
  }
  function sketchDetail(id) { return id === "" ? ({ available: false, regions: [], preview: { segments: [] } }) : workspace.sketchDetails(id) }
  function solveStatus(status) {
    if (status === "solved") return root.copy("Solved", "已求解")
    if (status === "underConstrained") return root.copy("Under-constrained", "約束不足")
    if (status === "overConstrained") return root.copy("Conflicting constraints", "約束衝突")
    if (status === "didNotConverge" || status === "invalidModel") return root.copy("Solve failed", "求解失敗")
    return status
  }
  function addPadForSelection() {
    var detail = sketchDetail(workspace.selectedBody)
    if (!detail.available || !detail.regions || detail.regions.length === 0 || workspace.busy) return
    padSketchId = workspace.selectedBody
    padRegionId = detail.regions[0].stableId
    padLength.text = "20"
    padDialog.open()
  }
  component SolvedSketchPreview: Canvas {
    property var details: ({ preview: { segments: [] } })
    onDetailsChanged: requestPaint()
    onVisibleChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
      var ctx = getContext("2d"); ctx.reset(); ctx.fillStyle = root.Material.background; ctx.fillRect(0, 0, width, height)
      var segments = details.preview && details.preview.segments ? details.preview.segments : []
      if (segments.length === 0) return
      var minU = Infinity, maxU = -Infinity, minV = Infinity, maxV = -Infinity
      for (var i = 0; i < segments.length; ++i) { var s = segments[i]; if (s.kind === "line") { minU=Math.min(minU,s.startU,s.endU); maxU=Math.max(maxU,s.startU,s.endU); minV=Math.min(minV,s.startV,s.endV); maxV=Math.max(maxV,s.startV,s.endV) } else { minU=Math.min(minU,s.centerU-s.radius); maxU=Math.max(maxU,s.centerU+s.radius); minV=Math.min(minV,s.centerV-s.radius); maxV=Math.max(maxV,s.centerV+s.radius) } }
      var span = Math.max(maxU-minU, maxV-minV, .001), scale = Math.min((width-32)/span, (height-32)/span), ox=(width-(maxU-minU)*scale)/2-minU*scale, oy=(height-(maxV-minV)*scale)/2+maxV*scale
      ctx.strokeStyle = root.Material.accent; ctx.lineWidth = 2; ctx.beginPath()
      for (var j = 0; j < segments.length; ++j) { var item=segments[j]; if (item.kind === "line") { ctx.moveTo(ox+item.startU*scale, oy-item.startV*scale); ctx.lineTo(ox+item.endU*scale, oy-item.endV*scale) } else { ctx.moveTo(ox+(item.centerU+item.radius)*scale, oy-item.centerV*scale); ctx.arc(ox+item.centerU*scale, oy-item.centerV*scale, item.radius*scale, 0, Math.PI*2) } }
      ctx.stroke()
    }
  }
  onClosing: (close)=> { if(!allowClose && (workspace.dirty || workspace.busy)) { close.accepted=false; guard("close") } }
  header: ToolBar {
    objectName: "mainToolbar"
    implicitHeight: toolbarFlow.implicitHeight + topPadding + bottomPadding
    Flow { id: toolbarFlow; objectName: "toolbarFlow"; width: parent.width; padding: 8; spacing: 8
      Label { textFormat: Text.PlainText; text: root.copy("Precision CAD", "精準 CAD"); font.pixelSize: 20; font.bold: true; width: 230 }
      ToolButton { text: root.copy("Box", "方盒"); onClicked: boxDialog.open() }
      ToolButton { objectName: "newSketchButton"; text: root.copy("Sketch", "草圖"); enabled: !workspace.busy; onClicked: root.openSketchEditor("") }
      ToolButton { objectName: "newPadButton"; text: root.copy("Pad", "拉伸"); enabled: !workspace.busy && root.sketchDetail(workspace.selectedBody).available && root.sketchDetail(workspace.selectedBody).sourceFeatureId === workspace.selectedBody && root.sketchDetail(workspace.selectedBody).kind === "sketch" && root.sketchDetail(workspace.selectedBody).regions.length > 0; onClicked: root.addPadForSelection() }
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
  footer: ToolBar { id: provenanceBar; objectName: "provenanceBar"; padding: 8
    implicitHeight: provenanceText.implicitHeight + topPadding + bottomPadding
    contentItem: Label { id: provenanceText; objectName: "versionInfo"; width: provenanceBar.availableWidth
      textFormat: Text.PlainText; wrapMode: Text.Wrap; elide: Text.ElideNone
      text: root.copy("Version","版本").replace(/\n/g, " / ") + " " + buildVersion + "  ·  " + root.copy("Updated","更新時間").replace(/\n/g, " / ") + " " + buildTime
      Accessible.role: Accessible.StaticText; Accessible.name: text
    }
  }
  FileDialog { id: saveDialog; currentFolder: root.documentFolder; objectName: "saveDialog"; title: root.copy("Save native Precision CAD document","儲存原生精準 CAD 文件"); fileMode: FileDialog.SaveFile; nameFilters: [root.copy("Precision CAD documents","精準 CAD 文件") + " (*.pcad)"]; onAccepted: workspace.save(workspace.localPath(selectedFile)); onRejected: { root.deferredAction=""; root.saveForDeferred=false } }
  FileDialog { id: openDialog; currentFolder: root.documentFolder; title: root.copy("Open native Precision CAD document","開啟原生精準 CAD 文件"); fileMode: FileDialog.OpenFile; nameFilters: [root.copy("Precision CAD documents","精準 CAD 文件") + " (*.pcad)"]; onAccepted: workspace.open(workspace.localPath(selectedFile)) }
  FileDialog { id: vocabularyDialog; currentFolder: root.documentFolder; title: root.copy("Load local vocabulary","載入本機詞彙"); fileMode: FileDialog.OpenFile; nameFilters: [root.copy("Vocabulary JSON","詞彙 JSON") + " (*.json)"]; onAccepted: { root.vocabularyStatus = uiText.loadVocabulary(selectedFile) ? root.copy("Local vocabulary loaded.","已載入本機詞彙。") : root.copy("Local vocabulary was not loaded.","未能載入本機詞彙。") } }
  Connections { target: workspace
    function onSaveFinished(ok, message) { var next=root.saveForDeferred ? root.deferredAction : ""; root.deferredAction=""; root.saveForDeferred=false; if(ok && next!=="") root.perform(next) }
    function onDocumentChanged() { root.selectedLeft=""; root.selectedRight=""; if (dimensionsDialog.visible) dimensionsDialog.close(); if (sketchEditor.visible) sketchEditor.close(); root.editTargetId=""; root.sketchEditId="" }
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
  Dialog { id: sketchEditor; objectName: "sketchEditorDialog"; modal: true; width: Math.min(root.width * .92, 720); height: Math.min(root.height * .88, 680)
    title: root.copy(sketchEditId === "" ? "New guided sketch" : "Edit guided sketch", sketchEditId === "" ? "新增引導式草圖" : "編輯引導式草圖")
    standardButtons: Dialog.NoButton
    onClosed: root.sketchEditId = ""
    footer: DialogButtonBox {
      Button { objectName: "saveSketchButton"; text: root.copy("Save sketch", "儲存草圖"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        onClicked: {
          if (workspace.busy) return
          if (root.sketchEditId === "") workspace.addSketch(["xy", "yz", "zx"][sketchPlane.currentIndex], Number(sketchWidth.text), Number(sketchHeight.text), Number(sketchHoleRadius.text), Number(sketchHoleU.text), Number(sketchHoleV.text))
          else workspace.updateSketch(root.sketchEditId, ["xy", "yz", "zx"][sketchPlane.currentIndex], Number(sketchWidth.text), Number(sketchHeight.text), Number(sketchHoleRadius.text), Number(sketchHoleU.text), Number(sketchHoleV.text))
          sketchEditor.close()
        }
      }
      Button { objectName: "cancelSketchButton"; text: root.copy("Cancel", "取消"); onClicked: sketchEditor.close() }
    }
    ScrollView { anchors.fill: parent; contentWidth: availableWidth
      Column { width: parent.width - leftPadding - rightPadding; spacing: 12; padding: 18
        Label { objectName: "guidedSketchNotice"; width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WordWrap; text: root.copy("Guided sketch: one rectangle with one circular hole. Freehand drawing is not available in this editor.", "引導式草圖：一個長方形加一個圓孔。此編輯器未提供自由繪圖。") }
        GridLayout { columns: sketchEditor.width > 560 ? 2 : 1; columnSpacing: 12; rowSpacing: 8; width: parent.width
          Label { text: root.copy("Plane", "平面") }
          ComboBox { id: sketchPlane; objectName: "sketchPlane"; Layout.fillWidth: true; model: ["XY", "YZ", "ZX"]; Accessible.name: root.copy("Sketch plane", "草圖平面") }
          Label { text: root.copy("Rectangle width (mm)", "長方形闊度（毫米）") }
          TextField { id: sketchWidth; objectName: "sketchWidth"; Layout.fillWidth: true; validator: DoubleValidator { bottom: .001 } Accessible.name: root.copy("Rectangle width in millimetres", "長方形闊度，毫米") }
          Label { text: root.copy("Rectangle height (mm)", "長方形高度（毫米）") }
          TextField { id: sketchHeight; objectName: "sketchHeight"; Layout.fillWidth: true; validator: DoubleValidator { bottom: .001 } Accessible.name: root.copy("Rectangle height in millimetres", "長方形高度，毫米") }
          Label { text: root.copy("Hole radius (mm)", "圓孔半徑（毫米）") }
          TextField { id: sketchHoleRadius; objectName: "sketchHoleRadius"; Layout.fillWidth: true; validator: DoubleValidator { bottom: .001 } Accessible.name: root.copy("Hole radius in millimetres", "圓孔半徑，毫米") }
          Label { text: root.copy("Hole centre U (mm)", "圓孔中心 U（毫米）") }
          TextField { id: sketchHoleU; objectName: "sketchHoleU"; Layout.fillWidth: true; validator: DoubleValidator {} Accessible.name: root.copy("Hole centre U in millimetres", "圓孔中心 U，毫米") }
          Label { text: root.copy("Hole centre V (mm)", "圓孔中心 V（毫米）") }
          TextField { id: sketchHoleV; objectName: "sketchHoleV"; Layout.fillWidth: true; validator: DoubleValidator {} Accessible.name: root.copy("Hole centre V in millimetres", "圓孔中心 V，毫米") }
        }
        Label { objectName: "sketchSolvedState"; width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WordWrap
          property var details: root.sketchDetail(root.sketchEditId)
          text: root.sketchEditId === "" ? root.copy("The solved preview and degrees of freedom appear after the sketch is created.", "草圖建立後會顯示求解預覽和自由度。") : (details.status === "" || details.status === undefined ? root.copy("Sketch state unavailable.", "未能取得草圖狀態。") : root.copy("Solved state: ", "求解狀態：") + root.solveStatus(details.status) + " · " + root.copy("Degrees of freedom: ", "自由度：") + details.dof + (details.conflicts && details.conflicts.length ? " · " + root.copy("Conflicts: ", "衝突：") + details.conflicts.join(", ") : "")) }
        SolvedSketchPreview { id: sketchPreview; objectName: "sketchSolvedPreview"; width: parent.width; height: Math.min(260, Math.max(160, width * .42)); visible: root.sketchEditId !== ""; details: root.sketchDetail(root.sketchEditId) }
      }
    }
  }
  Dialog { id: padDialog; objectName: "padDialog"; modal: true; width: Math.min(root.width * .92, 520); height: Math.min(root.height * .7, 360); title: root.copy("Pad selected sketch region", "拉伸所選草圖區域"); standardButtons: Dialog.NoButton
    footer: DialogButtonBox {
      Button { objectName: "createPadButton"; text: root.copy("Create pad", "建立拉伸"); onClicked: { if (!workspace.busy) workspace.addPad(root.padSketchId, root.padRegionId, Number(padLength.text)); padDialog.close() } }
      Button { text: root.copy("Cancel", "取消"); onClicked: padDialog.close() }
    }
    ScrollView { anchors.fill: parent; contentWidth: availableWidth
      GridLayout { width: parent.width - 32; columns: padDialog.width > 420 ? 2 : 1; rowSpacing: 10; columnSpacing: 10
        Label { text: root.copy("Region", "區域") }
        ComboBox { id: padRegionSelector; objectName: "padRegionSelector"; Layout.fillWidth: true; model: root.sketchDetail(root.padSketchId).regions; textRole: "stableId"; currentIndex: 0; onCurrentIndexChanged: { if (currentIndex >= 0 && currentIndex < count) root.padRegionId = model[currentIndex].stableId } Accessible.name: root.copy("Sketch region", "草圖區域") }
        Label { text: root.copy("Length (mm)", "長度（毫米）") }
        TextField { id: padLength; objectName: "padLength"; Layout.fillWidth: true; validator: DoubleValidator { bottom: .001 } Accessible.name: root.copy("Pad length in millimetres", "拉伸長度，毫米") }
      }
    }
  }
  SplitView { objectName: "workspaceSplit"; anchors.fill: parent
    Pane { objectName: "modelPane"; SplitView.preferredWidth: 310
      ScrollView { id: modelScroll; objectName: "modelScroll"; anchors.fill: parent; contentWidth: availableWidth
      Column { objectName: "modelColumn"; width: modelScroll.availableWidth; height: Math.max(modelScroll.availableHeight, modelHeader.implicitHeight + editDimensionsButton.implicitHeight + editSketchButton.implicitHeight + selectionHelp.implicitHeight + spacing * 4 + 100); spacing: 8
        Label { id: modelHeader; textFormat: Text.PlainText; text: root.copy("Model tree","模型樹"); font.bold: true; font.pixelSize: 18 }
        ListView { id: tree; objectName: "modelTree"; width: parent.width; height: Math.max(0, parent.height - modelHeader.implicitHeight - editDimensionsButton.implicitHeight - editSketchButton.implicitHeight - selectionHelp.implicitHeight - parent.spacing * 4); model: workspace.features; clip: true
          delegate: ItemDelegate { id: modelDelegate; width: tree.width; highlighted: workspace.selectedBody === modelData.id
            Accessible.role: Accessible.ListItem
            Accessible.selected: workspace.selectedBody === modelData.id
            Accessible.name: modelData.label + (workspace.selectedBody === modelData.id ? " " + root.copy("Selected", "已選取") : "")
            text: (modelData.suppressed ? "⊘ " : "") + modelData.label + "  " + modelData.id.slice(0, 8)
            onClicked: { if(root.selectedLeft === modelData.id) root.selectedLeft = ""; else if(root.selectedRight === modelData.id) root.selectedRight = ""; else if(root.selectedLeft === "") root.selectedLeft = modelData.id; else root.selectedRight = modelData.id; workspace.selectBody(modelData.id) }
            contentItem: Row { id: modelRow; objectName: "modelRow"; width: modelDelegate.availableWidth; spacing: 8
              Label { id: modelRowLabel; objectName: "modelRowLabel"; textFormat: Text.PlainText; text: (workspace.selectedBody === modelData.id ? root.copy("Selected", "已選取") + " · " : "") + (modelData.suppressed ? "⊘ " : "") + modelData.label + "  " + modelData.id.slice(0,8); anchors.verticalCenter: parent.verticalCenter; width: Math.max(0, modelRow.width - includeBody.implicitWidth - modelRow.spacing); elide: Text.ElideRight }
              Switch { id: includeBody; objectName: "modelRowIncludeSwitch"; Accessible.name: root.copy("Include body","啟用實體"); checked: !modelData.suppressed; onToggled: workspace.suppressFeature(modelData.id, !checked) }
            }
          }
        }
        Button { id: editDimensionsButton; text: root.copy("Edit selected dimensions","編輯所選尺寸"); enabled: !workspace.busy && workspace.editableDimensions(workspace.selectedBody).editable; onClicked: root.openDimensions() }
        Button { id: editSketchButton; objectName: "editSketchButton"; text: root.copy("Edit selected sketch","編輯所選草圖"); enabled: !workspace.busy && workspace.editableSketch(workspace.selectedBody).editable; onClicked: root.openSketchEditor(workspace.selectedBody) }
        Label { id: selectionHelp; objectName: "selectionHelp"; width: parent.width; textFormat: Text.PlainText; text: root.copy("Pick two tree bodies for boolean operations.","揀兩個實體進行布林運算。"); wrapMode: Text.WordWrap; opacity: preferences.adhdMode ? 1 : .8 }
      }
      }
    }
    Pane { SplitView.fillWidth: true
      Item { id: viewport; objectName: "viewport"; anchors.fill: parent
        property var vertices: workspace.meshVertices
        property var indices: workspace.meshIndices
        property var selectedSketchDetails: root.sketchDetail(workspace.selectedBody)
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
        Frame { id: selectedSketchPreviewCard; objectName: "selectedSketchPreviewCard"; z: 2; anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 12; width: Math.min(300, parent.width - 24); height: Math.max(0, parent.height - viewControls.implicitHeight - 24)
          visible: viewport.selectedSketchDetails.available && viewport.selectedSketchDetails.kind === "sketch" && viewport.selectedSketchDetails.sourceFeatureId === workspace.selectedBody
          ScrollView { anchors.fill: parent; contentWidth: availableWidth
          Column { width: parent.width; spacing: 6
            Label { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WordWrap; font.bold: true; text: root.copy("Solved sketch preview", "已求解草圖預覽") }
            Label { width: parent.width; textFormat: Text.PlainText; wrapMode: Text.WordWrap; text: root.copy("State: ", "狀態：") + root.solveStatus(viewport.selectedSketchDetails.status) + " · " + root.copy("Degrees of freedom: ", "自由度：") + viewport.selectedSketchDetails.dof }
            SolvedSketchPreview { objectName: "selectedSketchSolvedPreview"; width: parent.width; height: 210; details: viewport.selectedSketchDetails }
          }
          }
        }
        Flow { id: viewControls; objectName: "viewControls"; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; spacing: 4
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
        Label { objectName: "inspectorNotice"; width: parent.width; textFormat: Text.PlainText; text: root.copy("CAM, FEA, advanced editing, and the complete settings and history suite are unfinished in this modelling slice.","此建模階段尚未完成 CAM、FEA、圓角編輯及完整歷史功能。"); wrapMode: Text.WordWrap; opacity: preferences.adhdMode ? 1 : .8 }
      }
      }
    }
  }
  Dialog { id: dimensionsDialog; title: root.copy("Edit dimensions","編輯尺寸"); modal: true; standardButtons: Dialog.NoButton
    footer: DialogButtonBox { Button { text: root.copy("OK","確定"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole } Button { text: root.copy("Cancel","取消"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole } }
    onAccepted: { if (root.editTargetId !== "" && !workspace.busy) workspace.updateDimensions(root.editTargetId, Number(dimA.text), root.dimensionType === "pad" ? 1 : Number(dimB.text), root.dimensionType === "box" ? Number(dimC.text) : 1); root.editTargetId="" }
    onRejected: root.editTargetId=""
    Grid { columns:2; padding:16; spacing:8
      Label { textFormat: Text.PlainText; text: root.dimensionType === "pad" ? root.copy("Length (mm)","長度（毫米）") : root.dimensionType === "cylinder" ? root.copy("Radius (mm)","半徑（毫米）") : root.copy("X (mm)","X（毫米）") }
      TextField { id:dimA; validator: DoubleValidator { bottom: 0.001 } }
      Label { visible: root.dimensionType !== "pad"; textFormat: Text.PlainText; text: root.dimensionType === "cylinder" ? root.copy("Height (mm)","高度（毫米）") : root.copy("Y (mm)","Y（毫米）") }
      TextField { id:dimB; visible: root.dimensionType !== "pad"; validator: DoubleValidator { bottom: 0.001 } }
      Label { visible: root.dimensionType === "box"; textFormat: Text.PlainText; text: root.copy("Z (mm)","Z（毫米）") }
      TextField { id:dimC; visible: root.dimensionType === "box"; validator: DoubleValidator { bottom: 0.001 } }
    }
  }
}
