import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import PressureOS 1.0

Item {
    id: root
    property bool opened: false
    signal closeRequested()

    readonly property color accent: assistant.statusLevel === "critical" ? Theme.red
                                    : (assistant.statusLevel === "warning" ? Theme.orange
                                       : (assistant.statusLevel === "success" ? Theme.green
                                          : Theme.blue))
    readonly property color accentSoft: Qt.rgba(accent.r, accent.g, accent.b, 0.10)

    anchors.fill: parent
    visible: opacity > 0.01
    opacity: opened ? 1 : 0
    z: 100

    Rectangle {
        anchors.fill: parent
        color: Theme.modalScrim
        opacity: root.opened ? 0.74 : 0
        TapHandler { onTapped: root.closeRequested() }
    }

    GlassPanel {
        id: drawer
        width: Math.min(448, Math.max(400, parent.width * 0.43))
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: app.keyboardVisible ? Math.min(348, root.height - 184) : 12
        radius: 28
        material: "modal"
        tint: root.accent
        tintStrength: 0.012
        x: root.opened ? root.width - width - 12 : root.width + 24

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 11

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: 14
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#58AEFF" }
                        GradientStop { position: 1; color: "#546FF0" }
                    }
                    VectorIcon {
                        anchors.centerIn: parent
                        width: 22
                        height: 22
                        name: "assistant"
                        color: "white"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text {
                        text: "Pressure 任务助手"
                        color: Theme.inkStrong
                        font.family: Theme.fontFamily
                        font.pixelSize: 17
                        font.weight: Font.Bold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: assistant.contextLabel
                        color: Theme.inkMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textMicro
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 72
                    Layout.preferredHeight: 36
                    radius: 12
                    color: app.expertMode ? "#EEEAFE" : "#EAF5FF"
                    border.width: 1
                    border.color: app.expertMode ? "#C8BFF8" : "#BEDCF3"
                    Text {
                        anchors.centerIn: parent
                        text: app.expertMode ? "专业模式" : "引导模式"
                        color: app.expertMode ? Theme.violet : Theme.blueDeep
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textMicro
                        font.weight: Font.DemiBold
                    }
                    TapHandler { onTapped: app.expertMode = !app.expertMode }
                }

                Item {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    VectorIcon {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        name: "close"
                        color: Theme.inkMuted
                    }
                    TapHandler { margin: 4; onTapped: root.closeRequested() }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.lineSoft
            }

            ScrollView {
                id: assistantScroll
                visible: !app.keyboardVisible
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                Column {
                    width: assistantScroll.availableWidth
                    spacing: 12

                    GlassPanel {
                        width: parent.width
                        height: recommendationColumn.implicitHeight + 28
                        radius: 20
                        elevated: false
                        panelColor: "#F4FAFFFF"
                        panelColorBottom: "#EDF6FCFF"
                        tint: root.accent
                        tintStrength: 0.03
                        borderColor: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.22)

                        Column {
                            id: recommendationColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 14
                            spacing: 8

                            Row {
                                spacing: 8
                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 8
                                    color: root.accentSoft
                                    VectorIcon {
                                        anchors.centerIn: parent
                                        width: 13
                                        height: 13
                                        name: assistant.recommendation.icon || "spark"
                                        color: root.accent
                                    }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: assistant.statusLevel === "critical" ? "安全警报"
                                          : (assistant.statusLevel === "warning" ? "需要处理"
                                             : (assistant.statusLevel === "success" ? "当前可继续" : "此刻建议"))
                                    color: root.accent
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.textSmall
                                    font.weight: Font.Bold
                                }
                            }

                            Text {
                                width: parent.width
                                text: assistant.recommendation.title || "正在理解当前页面"
                                color: Theme.inkStrong
                                font.family: Theme.fontFamily
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                width: parent.width
                                text: assistant.recommendation.summary || "助手会根据任务、设备和分析状态给出下一步。"
                                color: Theme.ink
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textBody
                                lineHeight: 1.30
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                width: parent.width
                                text: assistant.recommendation.reason || ""
                                visible: text !== ""
                                color: Theme.inkMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textSmall
                                lineHeight: 1.28
                                wrapMode: Text.WordWrap
                            }

                            Row {
                                width: parent.width
                                height: 46
                                spacing: 8
                                PremiumButton {
                                    visible: assistant.recommendation.hasAction === true
                                    width: visible ? parent.width - explainButton.width - parent.spacing : 0
                                    height: 46
                                    compact: true
                                    text: assistant.recommendation.actionText || "执行下一步"
                                    iconName: assistant.recommendation.icon || "arrow"
                                    variant: "primary"
                                    accent: root.accent
                                    onClicked: assistant.executeAction(assistant.recommendation.actionId)
                                }
                                PremiumButton {
                                    id: explainButton
                                    width: assistant.recommendation.hasAction === true ? 104 : parent.width
                                    height: 46
                                    compact: true
                                    text: "为什么"
                                    iconName: "help"
                                    variant: "secondary"
                                    onClicked: assistant.explainRecommendation()
                                }
                            }
                        }
                    }

                    GlassPanel {
                        width: parent.width
                        height: answerColumn.implicitHeight + 28
                        visible: assistant.currentAnswer.id !== undefined
                                 && assistant.currentAnswer.id !== ""
                        radius: 20
                        elevated: false
                        panelColor: "#FFFEFFFF"
                        panelColorBottom: "#F8FBFEFF"
                        borderColor: Theme.modalBorder

                        Column {
                            id: answerColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 14
                            spacing: 8

                            Row {
                                width: parent.width
                                Text {
                                    width: parent.width - 44
                                    text: assistant.currentAnswer.question
                                          ? "你的问题 · " + assistant.currentAnswer.question
                                          : "助手说明"
                                    color: Theme.blueDeep
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.textMicro
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Item {
                                    width: 44
                                    height: 32
                                    VectorIcon {
                                        anchors.centerIn: parent
                                        width: 13
                                        height: 13
                                        name: "close"
                                        color: Theme.inkFaint
                                    }
                                    TapHandler { onTapped: assistant.clearAnswer() }
                                }
                            }
                            Text {
                                width: parent.width
                                text: assistant.currentAnswer.title || ""
                                color: Theme.inkStrong
                                font.family: Theme.fontFamily
                                font.pixelSize: 15
                                font.weight: Font.Bold
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                width: parent.width
                                text: assistant.currentAnswer.body || ""
                                color: Theme.ink
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textSmall
                                lineHeight: 1.35
                                wrapMode: Text.WordWrap
                            }
                            Repeater {
                                model: assistant.currentAnswer.bullets || []
                                delegate: Row {
                                    width: answerColumn.width
                                    spacing: 8
                                    Rectangle {
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: Theme.blue
                                        anchors.top: parent.top
                                        anchors.topMargin: 6
                                    }
                                    Text {
                                        width: parent.width - 14
                                        text: modelData
                                        color: Theme.inkMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.textMicro
                                        lineHeight: 1.28
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                            Rectangle {
                                width: parent.width
                                height: cautionText.implicitHeight + 18
                                radius: 12
                                color: Theme.orangeSoft
                                visible: (assistant.currentAnswer.caution || "") !== ""
                                Text {
                                    id: cautionText
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.margins: 9
                                    text: "注意：" + (assistant.currentAnswer.caution || "")
                                    color: "#805127"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.textMicro
                                    wrapMode: Text.WordWrap
                                }
                            }
                            PremiumButton {
                                width: parent.width
                                height: 46
                                visible: (assistant.currentAnswer.actionId || "") !== ""
                                compact: true
                                text: assistant.currentAnswer.actionText || "前往相关页面"
                                iconName: "arrow"
                                variant: "secondary"
                                onClicked: assistant.executeAction(assistant.currentAnswer.actionId)
                            }
                            Text {
                                width: parent.width
                                text: "依据 · " + (assistant.currentAnswer.source || "PressureOS 内置操作手册")
                                color: Theme.inkFaint
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textMicro
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Item {
                        width: parent.width
                        height: 22
                        Text {
                            text: "你可能想问"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textBody
                            font.weight: Font.Bold
                        }
                        Text {
                            anchors.right: parent.right
                            text: "随页面和阶段变化"
                            color: Theme.inkFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMicro
                        }
                    }

                    Repeater {
                        model: assistant.quickQuestions
                        delegate: GlassPanel {
                            width: assistantScroll.availableWidth
                            height: 48
                            radius: 15
                            elevated: false
                            interactive: true
                            panelColor: "#F8FCFFFF"
                            panelColorBottom: "#EFF7FCFF"
                            accessibleName: modelData.title

                            Rectangle {
                                x: 11
                                anchors.verticalCenter: parent.verticalCenter
                                width: 30
                                height: 30
                                radius: 10
                                color: Theme.blueSoft
                                VectorIcon {
                                    anchors.centerIn: parent
                                    width: 15
                                    height: 15
                                    name: modelData.icon || "help"
                                    color: Theme.blue
                                }
                            }
                            Text {
                                x: 51
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 82
                                text: modelData.title
                                color: Theme.ink
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textSmall
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            VectorIcon {
                                anchors.right: parent.right
                                anchors.rightMargin: 13
                                anchors.verticalCenter: parent.verticalCenter
                                width: 14
                                height: 14
                                name: "arrow"
                                color: Theme.inkFaint
                            }
                            onClicked: assistant.ask(modelData.id)
                        }
                    }

                    Item {
                        width: parent.width
                        height: 22
                        visible: assistant.history.length > 0
                        Text {
                            text: "本任务最近记录"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textBody
                            font.weight: Font.Bold
                        }
                        Text {
                            anchors.right: parent.right
                            text: assistant.history.length + " 条"
                            color: Theme.inkFaint
                            font.family: Theme.numberFont
                            font.pixelSize: Theme.textMicro
                        }
                    }

                    Repeater {
                        model: Math.min(2, assistant.history.length)
                        delegate: Rectangle {
                            width: assistantScroll.availableWidth
                            height: 46
                            radius: 13
                            color: "#EDF4F8"
                            readonly property var historyItem: assistant.history[index]
                            Text {
                                x: 12
                                y: 8
                                width: parent.width - 96
                                text: parent.historyItem.title || "助手记录"
                                color: Theme.inkMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textMicro
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.rightMargin: 12
                                y: 8
                                text: parent.historyItem.timeText || ""
                                color: Theme.inkFaint
                                font.family: Theme.numberFont
                                font.pixelSize: Theme.textMicro
                            }
                            Text {
                                x: 12
                                y: 25
                                width: parent.width - 24
                                text: parent.historyItem.body || ""
                                color: Theme.inkFaint
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textMicro
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: app.keyboardVisible ? 64 : 54
                radius: 16
                color: app.keyboardVisible ? "#FCFEFFFF" : "#F1F7FB"
                border.width: app.keyboardVisible ? 2 : 1
                border.color: questionInput.activeFocus ? Theme.blue : Theme.line

                TextField {
                    id: questionInput
                    anchors.left: parent.left
                    anchors.right: sendButton.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 13
                    anchors.rightMargin: 7
                    readOnly: true
                    placeholderText: "输入任务、测量或分析问题"
                    color: Theme.ink
                    placeholderTextColor: Theme.inkFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: app.keyboardVisible ? Theme.textBody : Theme.textSmall
                    verticalAlignment: TextInput.AlignVCenter
                    background: Item {}
                    TapHandler {
                        onTapped: {
                            questionInput.forceActiveFocus()
                            app.openKeyboard(questionInput, "text")
                        }
                    }
                }

                Rectangle {
                    id: sendButton
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    width: 44
                    height: 44
                    radius: 14
                    color: questionInput.text.trim().length > 0 ? Theme.blue : "#D6E3EC"
                    VectorIcon {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        name: "arrow"
                        color: "white"
                    }
                    TapHandler {
                        enabled: questionInput.text.trim().length > 0
                        onTapped: {
                            assistant.askText(questionInput.text)
                            questionInput.text = ""
                            assistantScroll.contentItem.contentY = 0
                        }
                    }
                }
            }

            RowLayout {
                visible: !app.keyboardVisible
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                spacing: 8
                Rectangle {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    radius: 8
                    color: Theme.greenSoft
                    VectorIcon {
                        anchors.centerIn: parent
                        width: 13
                        height: 13
                        name: "shield"
                        color: Theme.green
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: assistant.knowledgeReady
                          ? "本地知识库可离线使用 · 危险操作仍需人工确认"
                          : "知识库加载异常 · 仅保留状态提示"
                    color: Theme.inkMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textMicro
                    elide: Text.ElideRight
                }
            }
        }

        Behavior on x {
            NumberAnimation { duration: Theme.motionSlow; easing.type: Easing.OutCubic }
        }
        Behavior on anchors.bottomMargin {
            NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic }
        }
    }

    Component.onCompleted: {
        // Reuse the existing keyboard preview switch for deterministic
        // 1024x600 acceptance screenshots of the assistant's compact editor.
        if (opened && launchKeyboardPreviewMode !== "") {
            Qt.callLater(function() {
                questionInput.forceActiveFocus()
                app.openKeyboard(questionInput, launchKeyboardPreviewMode)
            })
        }
    }

    Behavior on opacity { NumberAnimation { duration: Theme.motionNormal } }
}
