import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import PrecisionCad

ApplicationWindow {
  id: root
  width: 1280; height: 820; visible: true
  title: "Precision CAD · " + buildVersion
  Material.theme: Material.Dark; Material.accent: Material.Blue
  property string selectedLeft: ""; property string selectedRight: ""
  property string deferredAction: ""
  function guard(action) { if (workspace.dirty) { deferredAction = action; unsavedDialog.open() } else perform(action) }
  function perform(action) { if(action === "new") workspace.newDocument(); else if(action === "open") openDialog.open(); else if(action === "close") Qt.quit() }
  onClosing: (close)=> { if(workspace.dirty) { close.accepted=false; guard("close") } }
  header: ToolBar {
    Row { anchors.fill: parent; anchors.margins: 8; spacing: 8
      Label { text: "Precision CAD"; font.pixelSize: 20; font.bold: true; width: 230 }
      Button { text: "Box"; onClicked: boxDialog.open() }
      Button { text: "New"; onClicked: root.guard("new") }
      Button { text: "Cylinder"; onClicked: cylinderDialog.open() }
      Button { text: "Union"; enabled: root.selectedLeft !== "" && root.selectedRight !== ""; onClicked: workspace.booleanOperation("union", root.selectedLeft, root.selectedRight) }
      Button { text: "Cut"; enabled: root.selectedLeft !== "" && root.selectedRight !== ""; onClicked: workspace.booleanOperation("cut", root.selectedLeft, root.selectedRight) }
      Button { text: "Intersect"; enabled: root.selectedLeft !== "" && root.selectedRight !== ""; onClicked: workspace.booleanOperation("intersection", root.selectedLeft, root.selectedRight) }
      Item { width: 1; height: 1; Layout.fillWidth: true }
      Button { text: "Undo"; onClicked: workspace.undo() }
      Button { text: "Redo"; onClicked: workspace.redo() }
      Button { text: "Fit"; onClicked: viewport.fit() }
      Button { text: "Save"; onClicked: saveDialog.open() }
      Button { text: "Open"; onClicked: root.guard("open") }
      Button { text: "Settings"; onClicked: settings.open() }
    }
  }
  FileDialog { id: saveDialog; title: "Save native Precision CAD document"; fileMode: FileDialog.SaveFile; nameFilters: ["Precision CAD (*.pcad)"]; onAccepted: workspace.save(selectedFile.toLocalFile()) }
  FileDialog { id: openDialog; title: "Open native Precision CAD document"; fileMode: FileDialog.OpenFile; nameFilters: ["Precision CAD (*.pcad)"]; onAccepted: workspace.open(selectedFile.toLocalFile()) }
  Dialog { id: unsavedDialog; title: preferences.dialogEmojis ? "⚠ Unsaved changes" : "Unsaved changes"; modal: true; standardButtons: Dialog.NoButton
    Column { padding: 20; spacing: 12; Label { text: "Save changes before continuing?" }; Row { spacing: 8
      Button { text: "Save"; onClicked: { saveDialog.open(); unsavedDialog.close() } }
      Button { text: "Discard"; onClicked: { var a=root.deferredAction; root.deferredAction=""; unsavedDialog.close(); root.perform(a) } }
      Button { text: "Cancel"; onClicked: { root.deferredAction=""; unsavedDialog.close() } }
    } }
  }
  Drawer { id: settings; edge: Qt.RightEdge; width: Math.min(390, root.width*.9); height: root.height
    Column { anchors.fill: parent; anchors.margins: 18; spacing: 12
      Label { text: "Preferences"; font.pixelSize: 22; font.bold: true }
      ComboBox { model: ["english","cantonese","bilingual"]; currentIndex: model.indexOf(preferences.languageMode); onActivated: preferences.setLanguageMode(currentText) }
      Label { text: "English tone" }; Slider { from: 0; to: 5; value: preferences.englishTone; onMoved: preferences.setEnglishTone(Math.round(value)) }
      Label { text: "Cantonese tone" }; Slider { from: 0; to: 5; value: preferences.cantoneseTone; onMoved: preferences.setCantoneseTone(Math.round(value)) }
      Switch { text: "Dialog emoji"; checked: preferences.dialogEmojis; onToggled: preferences.setDialogEmojis(checked) }
      ComboBox { model:["dark","light","system"]; currentIndex:model.indexOf(preferences.theme); onActivated: preferences.setTheme(currentText) }
      Label { text: "Font scale" }; Slider { from:.8; to:1.5; value:preferences.fontScale; onMoved: preferences.setFontScale(value) }
      Switch { text:"Reduced motion"; checked:preferences.reducedMotion; onToggled:preferences.setReducedMotion(checked) }
      Switch { text:"ADHD mode"; checked:preferences.adhdMode; onToggled:preferences.setAdhdMode(checked) }
      Label { text: "Narration is stored but unavailable until a local voice runtime is integrated."; wrapMode: Text.WordWrap }
    }
  }
  Dialog { id: boxDialog; title: "Dimensioned box"; modal: true; standardButtons: Dialog.Ok | Dialog.Cancel
    onAccepted: workspace.addBox(Number(dx.text), Number(dy.text), Number(dz.text))
    Grid { columns: 2; spacing: 12; padding: 16
      Label { text: "X (mm)" }
      TextField { id: dx; text: "40"; validator: DoubleValidator { bottom: 0.001 } }
      Label { text: "Y (mm)" }
      TextField { id: dy; text: "30"; validator: DoubleValidator { bottom: 0.001 } }
      Label { text: "Z (mm)" }
      TextField { id: dz; text: "20"; validator: DoubleValidator { bottom: 0.001 } }
    }
  }
  Dialog { id: cylinderDialog; title: "Dimensioned cylinder"; modal: true; standardButtons: Dialog.Ok | Dialog.Cancel
    onAccepted: workspace.addCylinder(Number(radius.text), Number(height.text))
    Grid { columns: 2; spacing: 12; padding: 16
      Label { text: "Radius (mm)" }
      TextField { id: radius; text: "12"; validator: DoubleValidator { bottom: 0.001 } }
      Label { text: "Height (mm)" }
      TextField { id: height; text: "45"; validator: DoubleValidator { bottom: 0.001 } }
    }
  }
  SplitView { anchors.fill: parent
    Pane { SplitView.preferredWidth: 310
      Column { anchors.fill: parent; spacing: 8
        Label { text: "Model tree"; font.bold: true; font.pixelSize: 18 }
        ListView { id: tree; width: parent.width; height: parent.height - 170; model: workspace.features; clip: true
          delegate: ItemDelegate { width: tree.width; highlighted: root.selectedLeft === modelData.id || root.selectedRight === modelData.id
            text: (modelData.suppressed ? "⊘ " : "") + modelData.label + "  " + modelData.id.slice(0, 8)
            onClicked: { if(root.selectedLeft === modelData.id) root.selectedLeft = ""; else if(root.selectedRight === modelData.id) root.selectedRight = ""; else if(root.selectedLeft === "") root.selectedLeft = modelData.id; else root.selectedRight = modelData.id }
            contentItem: Row { spacing: 8
              Label { text: parent.text; anchors.verticalCenter: parent.verticalCenter; width: 224; elide: Text.ElideRight }
              Switch { checked: !modelData.suppressed; onToggled: workspace.suppressFeature(modelData.id, !checked) }
            }
          }
        }
        Button { text: "Edit selected dimensions"; enabled: root.selectedLeft !== ""; onClicked: dimensionsDialog.open() }
        Label { text: "Pick two tree bodies for boolean operations."; wrapMode: Text.WordWrap; opacity: .8 }
      }
    }
    Pane { SplitView.fillWidth: true
      MeshCanvas { id: viewport; anchors.fill: parent; vertices: workspace.meshVertices; indices: workspace.meshIndices
        MouseArea { anchors.fill: parent; property real lastX; property real lastY; acceptedButtons: Qt.LeftButton | Qt.RightButton
          onPressed: (m)=> {lastX=m.x;lastY=m.y}
          onPositionChanged: (m)=> { if(pressedButtons & Qt.LeftButton) viewport.orbit((m.x-lastX)*.45,(m.y-lastY)*.45); else if(pressedButtons & Qt.RightButton) viewport.pan(m.x-lastX,m.y-lastY); lastX=m.x;lastY=m.y }
          onWheel: (w)=> viewport.zoom = viewport.zoom * (w.angleDelta.y > 0 ? 1.12 : .89)
        }
      }
    }
    Pane { SplitView.preferredWidth: 290
      Column { anchors.fill: parent; spacing: 10
        Label { text: "Operation"; font.bold: true; font.pixelSize: 18 }
        Label { text: workspace.operationState; wrapMode: Text.WordWrap }
        Label { visible: workspace.errorMessage.length > 0; text: workspace.errorMessage; color: Material.color(Material.Red); wrapMode: Text.WordWrap }
        Button { text: "Cancel geometry"; enabled: workspace.operationState.indexOf("Regenerating") === 0; onClicked: workspace.cancel() }
        Label { text: "Measurements"; font.bold: true; font.pixelSize: 18 }
        Label { text: "Volume: " + workspace.volume; wrapMode: Text.WordWrap }
        Label { text: "Bounds: " + workspace.bounds; wrapMode: Text.WordWrap }
        Rectangle { width: parent.width; height: 1; color: "#555b66" }
        Label { text: "Version " + buildVersion + "\nUpdated " + buildTime; wrapMode: Text.WordWrap }
        Label { text: "CAM, FEA, fillet editing, native preferences, and full history are unfinished in this modelling slice."; wrapMode: Text.WordWrap; opacity: .8 }
      }
    }
  }
  Dialog { id: dimensionsDialog; title: "Edit dimensions"; modal: true; standardButtons: Dialog.Ok | Dialog.Cancel
    onAccepted: workspace.updateDimensions(root.selectedLeft, Number(dimA.text), Number(dimB.text), Number(dimC.text))
    Grid { columns:2; padding:16; spacing:8; Label{text:"First"}; TextField{id:dimA;text:"10"}; Label{text:"Second"};TextField{id:dimB;text:"10"};Label{text:"Third (box)"};TextField{id:dimC;text:"10"} }
  }
}
