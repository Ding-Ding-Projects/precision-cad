import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import PrecisionCad

ApplicationWindow {
  id: root
  width: 1280; height: 820; visible: true
  title: root.copy("Precision CAD","精準 CAD") + " · " + buildVersion
  Material.theme: preferences.theme === "dark" ? Material.Dark : preferences.theme === "light" ? Material.Light : Material.System
  Material.accent: preferences.accentColor
  font.pixelSize: 14 * preferences.fontScale
  readonly property int effectiveTheme: Material.theme
  property bool allowClose: false
  property bool saveForDeferred: false
  property string vocabularyStatus: ""
  property string selectedLeft: ""; property string selectedRight: ""
  property string deferredAction: ""
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
  onClosing: (close)=> { if(!allowClose && (workspace.dirty || workspace.busy)) { close.accepted=false; guard("close") } }
  header: ToolBar {
    implicitHeight: children[0].implicitHeight + 16
    Flow { width: parent.width; padding: 8; spacing: 8
      Label { textFormat: Text.PlainText; text: root.copy("Precision CAD", "精準 CAD"); font.pixelSize: 20; font.bold: true; width: 230 }
      Button { text: root.copy("Box", "方盒"); onClicked: boxDialog.open() }
      Button { text: root.copy("New", "新檔"); onClicked: root.guard("new") }
      Button { text: root.copy("Cylinder", "圓柱"); onClicked: cylinderDialog.open() }
      Button { text: root.copy("Union","合併"); enabled: root.selectedLeft !== "" && root.selectedRight !== ""; onClicked: workspace.booleanOperation("union", root.selectedLeft, root.selectedRight) }
      Button { text: root.copy("Cut","切除"); enabled: root.selectedLeft !== "" && root.selectedRight !== ""; onClicked: workspace.booleanOperation("cut", root.selectedLeft, root.selectedRight) }
      Button { text: root.copy("Intersect","相交"); enabled: root.selectedLeft !== "" && root.selectedRight !== ""; onClicked: workspace.booleanOperation("intersection", root.selectedLeft, root.selectedRight) }

      Button { text: root.copy("Undo", "復原"); onClicked: workspace.undo() }
      Button { text: root.copy("Redo", "重做"); onClicked: workspace.redo() }
      Button { text: root.copy("Fit", "置中"); onClicked: viewport.fit() }
      Button { text: root.copy("Save", "儲存"); onClicked: { root.deferredAction=""; root.saveForDeferred=false; saveDialog.open() } }
      Button { text: root.copy("Open", "開啟"); onClicked: root.guard("open") }
      Button { text: root.copy("Settings", "設定"); onClicked: settings.open() }
    }
  }
  FileDialog { id: saveDialog; objectName: "saveDialog"; title: root.copy("Save native Precision CAD document","儲存原生精準 CAD 文件"); fileMode: FileDialog.SaveFile; nameFilters: [root.copy("Precision CAD documents","精準 CAD 文件") + " (*.pcad)"]; onAccepted: workspace.save(workspace.localPath(selectedFile)); onRejected: { root.deferredAction=""; root.saveForDeferred=false } }
  FileDialog { id: openDialog; title: root.copy("Open native Precision CAD document","開啟原生精準 CAD 文件"); fileMode: FileDialog.OpenFile; nameFilters: [root.copy("Precision CAD documents","精準 CAD 文件") + " (*.pcad)"]; onAccepted: workspace.open(workspace.localPath(selectedFile)) }
  FileDialog { id: vocabularyDialog; title: root.copy("Load local vocabulary","載入本機詞彙"); fileMode: FileDialog.OpenFile; nameFilters: [root.copy("Vocabulary JSON","詞彙 JSON") + " (*.json)"]; onAccepted: { root.vocabularyStatus = uiText.loadVocabulary(selectedFile) ? root.copy("Local vocabulary loaded.","已載入本機詞彙。") : root.copy("Local vocabulary was not loaded.","未能載入本機詞彙。") } }
  Connections { target: workspace
    function onSaveFinished(ok, message) { var next=root.saveForDeferred ? root.deferredAction : ""; root.deferredAction=""; root.saveForDeferred=false; if(ok && next!=="") root.perform(next) }
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
  SplitView { anchors.fill: parent
    Pane { SplitView.preferredWidth: 310
      Column { anchors.fill: parent; spacing: 8
        Label { textFormat: Text.PlainText; text: root.copy("Model tree","模型樹"); font.bold: true; font.pixelSize: 18 }
        ListView { id: tree; width: parent.width; height: parent.height - 170; model: workspace.features; clip: true
          delegate: ItemDelegate { width: tree.width; highlighted: root.selectedLeft === modelData.id || root.selectedRight === modelData.id
            text: (modelData.suppressed ? "⊘ " : "") + modelData.label + "  " + modelData.id.slice(0, 8)
            onClicked: { if(root.selectedLeft === modelData.id) root.selectedLeft = ""; else if(root.selectedRight === modelData.id) root.selectedRight = ""; else if(root.selectedLeft === "") root.selectedLeft = modelData.id; else root.selectedRight = modelData.id; workspace.selectBody(modelData.id) }
            contentItem: Row { spacing: 8
              Label { textFormat: Text.PlainText; text: (modelData.suppressed ? "⊘ " : "") + modelData.label + "  " + modelData.id.slice(0,8); anchors.verticalCenter: parent.verticalCenter; width: 224; elide: Text.ElideRight }
              Switch { Accessible.name: root.copy("Include body","啟用實體"); checked: !modelData.suppressed; onToggled: workspace.suppressFeature(modelData.id, !checked) }
            }
          }
        }
        Button { text: root.copy("Edit selected dimensions","編輯所選尺寸"); enabled: root.selectedLeft !== ""; onClicked: dimensionsDialog.open() }
        Label { objectName: "selectionHelp"; width: parent.width; textFormat: Text.PlainText; text: root.copy("Pick two tree bodies for boolean operations.","揀兩個實體進行布林運算。"); wrapMode: Text.WordWrap; opacity: preferences.adhdMode ? 1 : .8 }
      }
    }
    Pane { SplitView.fillWidth: true
      MeshCanvas { id: viewport; objectName: "viewport"; backgroundColor: Material.background; foregroundColor: Material.foreground; bodyColor: Material.accent; emptyText: root.copy("No regenerated mesh","尚未生成網格"); anchors.fill: parent; vertices: workspace.meshVertices; indices: workspace.meshIndices
        MouseArea { anchors.fill: parent; property real lastX; property real lastY; acceptedButtons: Qt.LeftButton | Qt.RightButton
          onPressed: (m)=> {lastX=m.x;lastY=m.y}
          onPositionChanged: (m)=> { if(pressedButtons & Qt.LeftButton) viewport.orbit((m.x-lastX)*.45,(m.y-lastY)*.45); else if(pressedButtons & Qt.RightButton) viewport.pan(m.x-lastX,m.y-lastY); lastX=m.x;lastY=m.y }
          onWheel: (w)=> viewport.zoom = viewport.zoom * (w.angleDelta.y > 0 ? 1.12 : .89)
        }
      }
    }
    Pane { SplitView.preferredWidth: 290
      Column { anchors.fill: parent; spacing: 10
        Label { textFormat: Text.PlainText; text: root.copy("Operation","運算"); font.bold: true; font.pixelSize: 18 }
        Label { textFormat: Text.PlainText; text: workspace.busy ? root.copy("Regenerating","重新生成中") : workspace.operationState === "Ready" ? root.copy("Ready","就緒") : workspace.operationState === "Cancelled" ? root.copy("Cancelled","已取消") : root.copy("Failed","未能完成"); wrapMode: Text.WordWrap }
        Label { visible: workspace.errorMessage.length > 0; text: workspace.errorMessage; color: Material.color(Material.Red); wrapMode: Text.WordWrap }
        Button { text: root.copy("Cancel geometry","取消幾何運算"); enabled: workspace.busy; onClicked: workspace.cancel() }
        Label { textFormat: Text.PlainText; text: root.copy("Measurements","量度"); font.bold: true; font.pixelSize: 18 }
        Label { textFormat: Text.PlainText; text: root.copy("Volume:","體積：") + " " + workspace.volume; wrapMode: Text.WordWrap }
        Label { textFormat: Text.PlainText; text: root.copy("Bounds:","邊界：") + " " + workspace.bounds; wrapMode: Text.WordWrap }
        Rectangle { width: parent.width; height: 1; color: "#555b66" }
        Label { textFormat: Text.PlainText; text: root.copy("Version","版本") + " " + buildVersion + "\n" + root.copy("Updated","更新時間") + " " + buildTime; wrapMode: Text.WordWrap }
        Label { textFormat: Text.PlainText; text: root.copy("CAM, FEA, fillet editing, native preferences, and full history are unfinished in this modelling slice.","此建模階段尚未完成 CAM、FEA、圓角編輯及完整歷史功能。"); wrapMode: Text.WordWrap; opacity: preferences.adhdMode ? 1 : .8 }
      }
    }
  }
  Dialog { id: dimensionsDialog; title: root.copy("Edit dimensions","編輯尺寸"); modal: true; standardButtons: Dialog.NoButton
    footer: DialogButtonBox { Button { text: root.copy("OK","確定"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole } Button { text: root.copy("Cancel","取消"); DialogButtonBox.buttonRole: DialogButtonBox.RejectRole } }
    onAccepted: workspace.updateDimensions(root.selectedLeft, Number(dimA.text), Number(dimB.text), Number(dimC.text))
    Grid { columns:2; padding:16; spacing:8
      Label { textFormat: Text.PlainText; text: root.copy("First","第一尺寸") }
      TextField { id:dimA; text:"10" }
      Label { textFormat: Text.PlainText; text: root.copy("Second","第二尺寸") }
      TextField { id:dimB; text:"10" }
      Label { textFormat: Text.PlainText; text: root.copy("Third (box)","第三尺寸（方盒）") }
      TextField { id:dimC; text:"10" }
    }
  }
}
