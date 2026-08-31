import QtQuick 2.15
import QtQuick.Controls 2.15
import PressureOS 1.0

Item {
    id: root
    objectName: "createTaskDialog"
    property bool opened: false
    property int selectedIndex: 0
    signal taskCreated()
    visible: opened
    z: 120

    Rectangle {
        anchors.fill: parent
        color: Theme.modalScrim
        z: 0
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            // Keep partially completed task data when the user taps outside.
            // The explicit close/cancel controls remain the only dismiss path.
        }
    }
    GlassPanel {
        id: dialogPanel
        objectName: "createTaskPanel"
        z: 1
        anchors.horizontalCenter: parent.horizontalCenter
        y: app.keyboardVisible ? 4 : Math.round((parent.height-height)/2)
        width: Math.min(850, parent.width-32)
        height: app.keyboardVisible ? 210 : Math.min(450, parent.height-20)
        radius: 28
        material: "modal"
        tintStrength: 0.012
        Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        // Absorb taps on blank areas inside the modal. Without this layer the
        // full-screen backdrop can also receive the same touch and close the
        // dialog immediately on the Raspberry Pi touch stack.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            preventStealing: true
        }
        Item {
            anchors.fill: parent; anchors.margins: app.keyboardVisible ? 16 : 20
            Text { text:"创建新任务";color:Theme.inkStrong;font.family:Theme.fontFamily;font.pixelSize:app.keyboardVisible?19:22;font.weight:Font.Bold }
            Text { visible:!app.keyboardVisible;y:31;text:"选择模板只是确定流程；本次任务名称由你单独填写，可重复使用同一模板。";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textBody }
            StatusChip { visible:app.keyboardVisible;y:29;text:templateRepo.templates[root.selectedIndex]?templateRepo.templates[root.selectedIndex].name:"已选模板";accent:Theme.violet;iconName:"task" }
            GlassPanel { anchors.right:parent.right; y:0;width:48;height:48;radius:16;material:"clear";interactive:true;elevated:false;accessibleName:"关闭";VectorIcon{anchors.centerIn:parent;width:16;height:16;name:"close";color:Theme.inkMuted}onClicked:root.opened=false }

            Text { visible:!app.keyboardVisible;y:59;text:"1　选择任务模板";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.DemiBold }
            Grid {
                id: templateGrid
                visible:!app.keyboardVisible
                y:81; width:parent.width; columns:2; columnSpacing:10; rowSpacing:8
                Repeater {
                    model: templateRepo.templates
                    delegate: Rectangle {
                        width: (templateGrid.width-templateGrid.columnSpacing)/2; height: 90; radius:17
                        color: root.selectedIndex===index ? "#EAF5FF" : "#F8FBFE"
                        border.width: root.selectedIndex===index ? 2 : 1
                        border.color: root.selectedIndex===index ? modelData.accent : Theme.line
                        Rectangle { x:12;y:11;width:34;height:34;radius:11;color:modelData.accent;opacity:.95;VectorIcon{anchors.centerIn:parent;width:18;height:18;name:modelData.icon;color:"white"} }
                        Text { x:55;y:10;width:parent.width-90;text:modelData.name;color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.DemiBold;elide:Text.ElideRight }
                        Text { x:55;y:32;text:modelData.category+" · "+modelData.duration;color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro }
                        Text { x:12;y:55;width:parent.width-24;height:27;text:modelData.description;wrapMode:Text.WordWrap;maximumLineCount:2;elide:Text.ElideRight;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;lineHeight:1.2 }
                        Rectangle { visible:root.selectedIndex===index;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:10;width:23;height:23;radius:8;color:modelData.accent;VectorIcon{anchors.centerIn:parent;width:12;height:12;name:"check";color:"white"} }
                        TapHandler { onTapped: root.selectedIndex=index }
                    }
                }
            }
            Text { y:app.keyboardVisible?54:278;text:(app.keyboardVisible?"任务名称":"2　为本次任务命名");color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.DemiBold }
            Rectangle {
                x:0;y:app.keyboardVisible?76:302;width:app.keyboardVisible?parent.width:500;height:app.keyboardVisible?48:54;radius:16;color:"#F9FCFF";border.width:1;border.color:nameField.activeFocus?Theme.blue:Theme.line
                TextField {
                    id:nameField;anchors.fill:parent;anchors.leftMargin:16;anchors.rightMargin:16
                    placeholderText:"例如：8月9日第二轮压力—质量实验";color:Theme.ink;placeholderTextColor:Theme.inkFaint
                    font.family:Theme.fontFamily;font.pixelSize:13;selectByMouse:true
                    background:Item{}
                    onPressed: app.openKeyboard(nameField,"text")
                }
            }
            Text { visible:!app.keyboardVisible;x:515;y:307;width:260;text:"名称用于区分同一模板的多次实验，创建后会立即建立独立任务胶囊。";wrapMode:Text.WordWrap;color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;lineHeight:1.3 }
            Row {
                anchors.right:parent.right;anchors.bottom:parent.bottom;spacing:10
                PremiumButton { width:104;height:48;compact:true;text:"取消";variant:"ghost";onClicked:root.opened=false }
                PremiumButton { width:166;height:48;compact:true;text:"创建并进入";iconName:"arrow";variant:"primary";onClicked:{const t=templateRepo.templates[root.selectedIndex];if(t&&tasks.createTask(t.id,nameField.text)){root.opened=false;nameField.text="";root.taskCreated()}} }
            }
        }
    }
    onOpenedChanged: {
        if (opened) {
            selectedIndex = 0
            nameField.text = ""
        } else {
            app.hideKeyboard()
        }
    }
    Component.onCompleted: {
        if (!launchCreateTaskDialog)
            return
        opened = true
        if (launchKeyboardPreviewMode !== "") {
            Qt.callLater(function() {
                nameField.forceActiveFocus()
                app.openKeyboard(nameField, launchKeyboardPreviewMode)
            })
        }
    }
}
