import QtQuick 2.15
import QtQuick.Controls 2.15
import PressureOS 1.0

Item {
    id: root
    property bool opened: false
    property string selectedMode: "auto-index"
    property int suggestedPoints: 5
    property string activeField: "name"
    signal taskCreated()
    visible: opened
    z: 125

    function defaultName() {
        return "快速测量 · " + Qt.formatDateTime(new Date(), "M月d日 HH:mm")
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.modalScrim
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }
    }

    GlassPanel {
        id: dialogPanel
        anchors.horizontalCenter: parent.horizontalCenter
        y: app.keyboardVisible ? 4 : Math.round((parent.height-height)/2)
        width: Math.min(820, parent.width-32)
        height: app.keyboardVisible ? 208 : Math.min(480, parent.height-18)
        radius: 28
        material: "modal"
        tint: Theme.blue
        tintStrength: 0.012
        Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; preventStealing: true }

        Item {
            anchors.fill: parent
            anchors.margins: app.keyboardVisible ? 16 : 20

            Text {
                text: app.keyboardVisible ? "编辑快速任务" : "快速空白任务"
                color: Theme.inkStrong
                font.family: Theme.fontFamily
                font.pixelSize: app.keyboardVisible ? 18 : 22
                font.weight: Font.Bold
            }
            Text {
                visible: !app.keyboardVisible
                y: 31
                text: "不做模板也能立即开始：逐点自动保存，3 个点后即可制表、试拟合和导出。"
                color: Theme.inkMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textBody
            }
            StatusChip {
                visible: app.keyboardVisible
                y: 29
                text: root.activeField === "name" ? "任务名称" : (root.activeField === "xName" ? "横轴名称" : "横轴单位")
                accent: Theme.blue
                iconName: "edit"
            }
            GlassPanel {
                anchors.right: parent.right; y: 0
                width: 48; height: 48; radius: 16; material:"clear";interactive:true;elevated:false;accessibleName:"关闭"
                VectorIcon { anchors.centerIn: parent; width: 15; height: 15; name: "close"; color: Theme.inkMuted }
                onClicked: root.opened = false
            }

            Rectangle {
                visible: !app.keyboardVisible
                x: 0; y: 58; width: parent.width; height: 52; radius: 16
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "#E9F6FF" }
                    GradientStop { position: 1; color: "#F1F3FF" }
                }
                Rectangle { x: 11; anchors.verticalCenter: parent.verticalCenter; width: 31; height: 31; radius: 10; color: Theme.blue; VectorIcon { anchors.centerIn: parent; width: 16; height: 16; name: "spark"; color: "white" } }
                Text { x: 52; y: 8; text: "先记录，再整理"; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall; font.weight: Font.DemiBold }
                Text { x: 52; y: 27; width: parent.width-64; text: "适合临时测量、现场摸底和尚未固化流程的实验；之后仍可删除误测点并反复导出。"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro; elide: Text.ElideRight }
            }

            Text { visible: !app.keyboardVisible; y: 122; text: "1　选择最省事的采点方式"; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }
            Row {
                visible: !app.keyboardVisible
                y: 144; width: parent.width; height: 78; spacing: 10
                Repeater {
                    model: [
                        { mode: "auto-index", icon: "record", title: "自动序号", tag: "最快 · 推荐", detail: "不用填横轴，连续点击采集；X 自动记为 1、2、3…" },
                        { mode: "manual-x", icon: "edit", title: "自定义横轴", tag: "更灵活", detail: "填写质量、时间或位移等变量；每个点录入一个 X 值。" }
                    ]
                    delegate: Rectangle {
                        width: (parent.width-parent.spacing)/2; height: parent.height; radius: 17
                        color: root.selectedMode === modelData.mode ? "#EAF5FF" : "#F8FBFE"
                        border.width: root.selectedMode === modelData.mode ? 2 : 1
                        border.color: root.selectedMode === modelData.mode ? Theme.blue : Theme.line
                        Rectangle { x: 11; y: 11; width: 35; height: 35; radius: 11; color: root.selectedMode === modelData.mode ? Theme.blue : "#E7F0F7"; VectorIcon { anchors.centerIn: parent; width: 17; height: 17; name: modelData.icon; color: root.selectedMode === modelData.mode ? "white" : Theme.inkMuted } }
                        Text { x: 56; y: 9; text: modelData.title; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 13; font.weight: Font.DemiBold }
                        StatusChip { anchors.right: parent.right; anchors.rightMargin: 10; y: 8; height: 23; text: modelData.tag; accent: modelData.mode === "auto-index" ? Theme.green : Theme.violet; dot: false }
                        Text { x: 56; y: 35; width: parent.width-68; text: modelData.detail; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; lineHeight: 1.2 }
                        TapHandler { onTapped: root.selectedMode = modelData.mode }
                    }
                }
            }

            Text { visible: !app.keyboardVisible; y: 235; text: "2　补充最少的信息"; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.DemiBold }

            Rectangle {
                visible: !app.keyboardVisible || root.activeField === "name"
                x: 0; y: app.keyboardVisible ? 58 : 257
                width: app.keyboardVisible ? parent.width : 310; height: 50; radius: 15
                color: "#F9FCFF"; border.width: 1; border.color: nameField.activeFocus ? Theme.blue : Theme.line
                Text { x: 12; y: 5; text: "任务名称"; color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: 9 }
                TextField {
                    id: nameField; x: 8; y: 16; width: parent.width-16; height: 32
                    color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: 13; selectByMouse: true
                    background: Item {}
                    onPressed: { root.activeField = "name"; app.openKeyboard(nameField, "text") }
                }
            }

            Rectangle {
                visible: root.selectedMode === "manual-x" && (!app.keyboardVisible || root.activeField === "xName")
                x: app.keyboardVisible ? 0 : 321; y: app.keyboardVisible ? 58 : 257
                width: app.keyboardVisible ? parent.width : 208; height: 50; radius: 15
                color: "#F9FCFF"; border.width: 1; border.color: xNameField.activeFocus ? Theme.blue : Theme.line
                Text { x: 12; y: 5; text: "横轴名称"; color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: 9 }
                TextField {
                    id: xNameField; x: 8; y: 16; width: parent.width-16; height: 32
                    text: "时间"; placeholderText: "如：质量、时间"; color: Theme.ink; placeholderTextColor: Theme.inkFaint
                    font.family: Theme.fontFamily; font.pixelSize: 13; selectByMouse: true; background: Item {}
                    onPressed: { root.activeField = "xName"; app.openKeyboard(xNameField, "text") }
                }
            }

            Rectangle {
                visible: root.selectedMode === "manual-x" && (!app.keyboardVisible || root.activeField === "xUnit")
                x: app.keyboardVisible ? 0 : 540; y: app.keyboardVisible ? 58 : 257
                width: app.keyboardVisible ? parent.width : 122; height: 50; radius: 15
                color: "#F9FCFF"; border.width: 1; border.color: xUnitField.activeFocus ? Theme.blue : Theme.line
                Text { x: 12; y: 5; text: "单位"; color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: 9 }
                TextField {
                    id: xUnitField; x: 8; y: 16; width: parent.width-16; height: 32
                    text: "s"; placeholderText: "如 g、s"; color: Theme.ink; placeholderTextColor: Theme.inkFaint
                    font.family: Theme.numberFont; font.pixelSize: 13; selectByMouse: true; background: Item {}
                    onPressed: { root.activeField = "xUnit"; app.openKeyboard(xUnitField, "text") }
                }
            }

            Rectangle {
                visible: !app.keyboardVisible && root.selectedMode === "auto-index"
                x: 321; y: 257; width: 341; height: 50; radius: 15; color: "#EEF9F5"
                Rectangle { x: 11; anchors.verticalCenter: parent.verticalCenter; width: 29; height: 29; radius: 9; color: Theme.green; VectorIcon { anchors.centerIn: parent; width: 14; height: 14; name: "check"; color: "white" } }
                Text { x: 49; y: 8; text: "无需再填写变量"; color: Theme.ink; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall; font.weight: Font.DemiBold }
                Text { x: 49; y: 27; text: "进入任务后直接点击“采集第 N 点”"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro }
            }

            Row {
                visible: !app.keyboardVisible
                y: 319; spacing: 8
                Text { width: 120; anchors.verticalCenter: parent.verticalCenter; text: "建议采集点数"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall }
                Repeater {
                    model: [5, 8, 10]
                    delegate: Rectangle {
                        width: 58; height: 48; radius: 15
                        color: root.suggestedPoints === modelData ? Theme.blue : "#EDF4F9"
                        Text { anchors.centerIn: parent; text: modelData + " 点"; color: root.suggestedPoints === modelData ? "white" : Theme.inkMuted; font.family: Theme.numberFont; font.pixelSize: Theme.textSmall; font.weight: Font.DemiBold }
                        TapHandler { onTapped: root.suggestedPoints = modelData }
                    }
                }
                Text { anchors.verticalCenter: parent.verticalCenter; text: "（达到 3 点即可提前分析）"; color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro }
            }

            Row {
                anchors.right: parent.right; anchors.bottom: parent.bottom; spacing: 9
                PremiumButton { width: 108; height: 48; compact: true; text: "取消"; variant: "ghost"; onClicked: root.opened = false }
                PremiumButton {
                    width: 180; height: 48; compact: true
                    text: root.selectedMode === "auto-index" ? "立即开始采集" : "创建并填写数据"
                    iconName: "arrow"; variant: "primary"
                    onClicked: {
                        if (tasks.createQuickTask(nameField.text, xNameField.text, xUnitField.text,
                                                  root.selectedMode, root.suggestedPoints)) {
                            root.opened = false
                            root.taskCreated()
                        }
                    }
                }
            }
        }
    }

    onOpenedChanged: {
        if (opened) {
            selectedMode = "auto-index"
            suggestedPoints = 5
            activeField = "name"
            nameField.text = defaultName()
            xNameField.text = "时间"
            xUnitField.text = "s"
        } else {
            app.hideKeyboard()
        }
    }
    Component.onCompleted: {
        if (launchQuickTaskDialog)
            opened = true
    }
}
