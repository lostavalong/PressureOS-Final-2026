import QtQuick 2.15
import PressureOS 1.0

Item {
    id: root
    objectName: "touchKeyboard"
    visible: true
    enabled: app.keyboardVisible
    opacity: app.keyboardVisible ? 1 : 0
    z: 300
    property bool uppercase: false
    readonly property string targetText: app.inputTarget ? String(app.inputTarget.text || "") : ""

    Behavior on opacity { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }

    function textLayout() {
        if (pinyin.chineseMode) {
            return [["q","w","e","r","t","y","u","i","o","p"],
                    ["a","s","d","f","g","h","j","k","l"],
                    ["中/英","z","x","c","v","b","n","m","退格"]]
        }
        return uppercase
            ? [["Q","W","E","R","T","Y","U","I","O","P"],
               ["A","S","D","F","G","H","J","K","L"],
               ["⇧","Z","X","C","V","B","N","M","退格"]]
            : [["q","w","e","r","t","y","u","i","o","p"],
               ["a","s","d","f","g","h","j","k","l"],
               ["⇧","z","x","c","v","b","n","m","退格"]]
    }

    function numericLayout() {
        return [["1","2","3","退格"], ["4","5","6","清空"],
                ["7","8","9","−"], ["+","0",".","完成"]]
    }

    function symbolLayout() {
        return [["!","@","#","$","%","^","&","*"],
                ["(",")","-","_","=","+","[","]"],
                ["{","}","/","\\",":",";","?","退格"]]
    }

    function currentLayout() {
        return app.keyboardMode === "numeric" ? numericLayout()
             : (app.keyboardMode === "symbols" ? symbolLayout() : textLayout())
    }

    function replaceComposition(value, typedLength) {
        for (let i = 0; i < typedLength; ++i)
            app.backspaceInput()
        if (value.length > 0)
            app.inputText(value)
    }

    function commitComposition() {
        const typedLength = pinyin.composition.length
        if (typedLength === 0)
            return
        const value = pinyin.takeFirstCandidate()
        replaceComposition(value, typedLength)
    }

    function chooseCandidate(index) {
        const typedLength = pinyin.composition.length
        if (typedLength === 0)
            return
        const value = pinyin.takeCandidate(index)
        replaceComposition(value, typedLength)
    }

    function toggleLanguage() {
        commitComposition()
        uppercase = false
        pinyin.setChineseMode(!pinyin.chineseMode)
        if (app.keyboardMode !== "text")
            app.setKeyboardMode("text")
    }

    function changeMode(mode) {
        if (mode !== "text")
            commitComposition()
        app.setKeyboardMode(mode)
    }

    function finishInput() {
        commitComposition()
        app.commitInput()
    }

    function activate(key) {
        if (key === "退格") {
            if (app.keyboardMode === "text" && pinyin.chineseMode && pinyin.composition.length > 0) {
                pinyin.backspace()
                app.backspaceInput()
            } else {
                app.backspaceInput()
            }
        } else if (key === "清空") {
            pinyin.clear()
            app.clearInput()
        } else if (key === "完成") {
            finishInput()
        } else if (key === "中/英") {
            toggleLanguage()
        } else if (key === "⇧") {
            uppercase = !uppercase
        } else if (key === "SPACE") {
            if (app.keyboardMode === "text" && pinyin.chineseMode && pinyin.composition.length > 0)
                commitComposition()
            else
                app.inputText(" ")
        } else if (key === "−") {
            app.inputText("-")
        } else if (app.keyboardMode === "text" && pinyin.chineseMode && /^[A-Za-z]$/.test(key)) {
            const previousLength = pinyin.composition.length
            pinyin.appendLetter(key)
            // Keep the pre-edit pinyin visible in the real input field.  The
            // chosen candidate replaces these letters instead of being added
            // after them, matching the behaviour users expect from a phone IME.
            if (pinyin.composition.length > previousLength)
                app.inputText(key.toLowerCase())
        } else {
            app.inputText(key)
        }
    }

    Connections {
        target: app
        function onKeyboardChanged() {
            if (!app.keyboardVisible)
                pinyin.clear()
        }
    }

    Rectangle {
        anchors.fill: parent
        // The page/dialog above the keyboard must remain legible on the
        // 7-inch display.  A light scrim still separates the layers without
        // turning the active input field into a dark, hard-to-read backdrop.
        color: "#140B2138"
        z: 0
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onClicked: {
                root.commitComposition()
                app.hideKeyboard()
            }
        }
    }

    GlassPanel {
        id: panel
        objectName: "touchKeyboardPanel"
        z: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: app.keyboardMode === "text" ? 340 : 318
        radius: 28
        material: "modal"
        tint: Theme.blue
        tintStrength: 0.012
        transform: Translate {
            y: app.keyboardVisible ? 0 : panel.height
            Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        }
        Behavior on height { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            preventStealing: true
        }

        Row {
            id: modeBar
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 22
            anchors.topMargin: 10
            spacing: 7

            Repeater {
                model: [
                    {m:"numeric", t:"123"},
                    {m:"text", t:pinyin.chineseMode ? "拼音" : "ABC"},
                    {m:"symbols", t:"#+="}
                ]
                delegate: Rectangle {
                    width: 64; height: 44; radius: 14
                    color: app.keyboardMode === modelData.m ? Theme.blue : "#74FFFFFF"
                    border.width: 1
                    border.color: app.keyboardMode === modelData.m ? "#8AC7FF" : "#8FFFFFFF"
                    scale: modeTap.pressed ? 0.96 : 1
                    Text {
                        anchors.centerIn: parent
                        text: modelData.t
                        color: app.keyboardMode === modelData.m ? "white" : Theme.inkMuted
                        font.family: modelData.m === "numeric" ? Theme.numberFont : Theme.fontFamily
                        font.pixelSize: Theme.textBody
                        font.weight: Font.DemiBold
                    }
                    TapHandler { id: modeTap; onTapped: root.changeMode(modelData.m) }
                    Behavior on scale { ScaleAnimator { duration: Theme.motionFast } }
                }
            }

            Rectangle {
                visible: app.keyboardMode === "text"
                width: 74; height: 44; radius: 14
                color: pinyin.chineseMode ? "#E4F2FF" : "#F1F5F8"
                border.width: 1; border.color: pinyin.chineseMode ? "#A8D4FA" : "#D8E3EB"
                Text {
                    anchors.centerIn: parent
                    text: "中 / 英"
                    color: pinyin.chineseMode ? Theme.blueDeep : Theme.inkMuted
                    font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall; font.weight: Font.Bold
                }
                TapHandler { onTapped: root.toggleLanguage() }
            }
        }

        Text {
            visible: app.keyboardMode !== "text"
            anchors.left: modeBar.right; anchors.leftMargin: 14
            anchors.verticalCenter: modeBar.verticalCenter
            text: "PressureOS 触屏键盘"
            color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall
        }

        Rectangle {
            id: inputPreview
            anchors.left: modeBar.right; anchors.leftMargin: 12
            anchors.right: collapseButton.left; anchors.rightMargin: 12
            anchors.verticalCenter: modeBar.verticalCenter
            height: 44; radius: 14
            color: "#F2FAFDFF"
            border.width: 1
            border.color: pinyin.composition.length > 0 ? "#91C9F5" : "#D5E5F0"
            clip: true

            Text {
                id: previewLabel
                x: 13
                anchors.verticalCenter: parent.verticalCenter
                text: "当前输入"
                color: Theme.blueDeep
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textMicro
                font.weight: Font.DemiBold
            }
            Rectangle {
                x: previewLabel.x + previewLabel.implicitWidth + 10
                anchors.verticalCenter: parent.verticalCenter
                width: 1; height: 22; color: Theme.line
            }
            Text {
                x: previewLabel.x + previewLabel.implicitWidth + 22
                anchors.right: parent.right; anchors.rightMargin: 13
                anchors.verticalCenter: parent.verticalCenter
                text: root.targetText.length > 0 ? root.targetText : "输入内容会同步显示在上方输入框"
                color: root.targetText.length > 0 ? Theme.inkStrong : Theme.inkFaint
                font.family: Theme.fontFamily
                font.pixelSize: root.targetText.length > 0 ? Theme.textBody : Theme.textSmall
                font.weight: root.targetText.length > 0 ? Font.DemiBold : Font.Normal
                elide: Text.ElideLeft
            }
        }

        Rectangle {
            id: collapseButton
            anchors.right: parent.right; anchors.rightMargin: 20
            anchors.verticalCenter: modeBar.verticalCenter
            width: 76; height: 44; radius: 14
            color: "#74FFFFFF"; border.width: 1; border.color: "#8FFFFFFF"
            Text {
                anchors.centerIn: parent; text: "收起"; color: Theme.inkMuted
                font.family: Theme.fontFamily; font.pixelSize: Theme.textBody
            }
            TapHandler {
                onTapped: {
                    root.commitComposition()
                    app.hideKeyboard()
                }
            }
        }

        Rectangle {
            id: candidateBar
            visible: app.keyboardMode === "text"
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: modeBar.bottom; anchors.topMargin: 6
            anchors.leftMargin: 22; anchors.rightMargin: 20
            height: 43; radius: 14
            color: "#B8F4F9FC"; border.width: 1; border.color: "#D5E5F0"
            clip: true

            Rectangle {
                id: compositionBadge
                anchors.left: parent.left; anchors.leftMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(88, Math.min(150, compositionText.implicitWidth + 24))
                height: 31; radius: 10
                color: pinyin.composition.length > 0 ? "#E2F1FF" : "transparent"
                Text {
                    id: compositionText
                    anchors.centerIn: parent
                    text: pinyin.composition.length > 0
                          ? pinyin.composition
                          : (pinyin.chineseMode ? "输入拼音" : "英文直输")
                    color: pinyin.composition.length > 0 ? Theme.blueDeep : Theme.inkFaint
                    font.family: Theme.numberFont; font.pixelSize: Theme.textSmall
                    font.weight: pinyin.composition.length > 0 ? Font.Bold : Font.Normal
                }
            }

            Rectangle {
                anchors.left: compositionBadge.right; anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                width: 1; height: 23; color: Theme.line
            }

            ListView {
                id: candidateList
                anchors.left: compositionBadge.right; anchors.leftMargin: 15
                anchors.right: parent.right; anchors.rightMargin: 7
                anchors.top: parent.top; anchors.bottom: parent.bottom
                orientation: ListView.Horizontal
                model: pinyin.candidates
                spacing: 6
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    height: 31; width: Math.max(50, candidateText.implicitWidth + 26)
                    anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                    radius: 10
                    color: candidateTap.pressed ? "#D6EAFE" : (index === 0 ? "#EDF6FF" : "#86FFFFFF")
                    border.width: index === 0 ? 1 : 0
                    border.color: "#BBDCF8"
                    Text {
                        id: candidateText; anchors.centerIn: parent; text: modelData
                        color: index === 0 ? Theme.blueDeep : Theme.ink
                        font.family: Theme.fontFamily; font.pixelSize: 14; font.weight: Font.DemiBold
                    }
                    TapHandler {
                        id: candidateTap
                        onTapped: root.chooseCandidate(index)
                    }
                }
            }

            Text {
                visible: pinyin.chineseMode && pinyin.composition.length > 0 && pinyin.candidates.length === 0
                anchors.left: compositionBadge.right; anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: "暂无词条 · 点空格保留拼音"
                color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall
            }
        }

        Column {
            id: keyRows
            anchors.left: parent.left; anchors.right: parent.right
            anchors.top: app.keyboardMode === "text" ? candidateBar.bottom : modeBar.bottom
            anchors.leftMargin: 22; anchors.rightMargin: 20
            anchors.topMargin: app.keyboardMode === "text" ? 7 : 10
            spacing: 6

            Repeater {
                model: root.currentLayout()
                delegate: Row {
                    id: keyRow
                    width: keyRows.width
                    height: app.keyboardMode === "numeric" ? 50 : 44
                    spacing: 7
                    readonly property var rowKeys: modelData

                    Repeater {
                        model: keyRow.rowKeys
                        delegate: Rectangle {
                            width: (keyRow.width - (keyRow.rowKeys.length - 1) * keyRow.spacing) / keyRow.rowKeys.length
                            height: keyRow.height; radius: 13
                            color: keyPress.pressed ? "#D7E9F7" : (modelData === "完成" ? Theme.blue : "white")
                            border.width: 1
                            border.color: modelData === "完成" ? "#54A8FF" : "#D7E5F0"
                            scale: keyPress.pressed ? 0.95 : 1
                            Text {
                                anchors.centerIn: parent; text: modelData
                                color: modelData === "完成" ? "white" : Theme.ink
                                font.family: /^[A-Za-z0-9]$/.test(String(modelData)) ? Theme.numberFont : Theme.fontFamily
                                font.pixelSize: app.keyboardMode === "numeric" ? 16 : 13
                                font.weight: Font.DemiBold
                            }
                            TapHandler { id: keyPress; onTapped: root.activate(String(modelData)) }
                            Behavior on scale { ScaleAnimator { duration: Theme.motionFast } }
                        }
                    }
                }
            }
        }

        Row {
            visible: app.keyboardMode !== "numeric"
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.leftMargin: 22; anchors.rightMargin: 20; anchors.bottomMargin: 10
            spacing: 8

            Rectangle {
                width: 94; height: 44; radius: 14
                color: "#74FFFFFF"; border.width: 1; border.color: "#8FFFFFFF"
                Text { anchors.centerIn: parent; text: "清空"; color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textBody }
                TapHandler { onTapped: root.activate("清空") }
            }
            Rectangle {
                width: parent.width - 284; height: 44; radius: 14
                color: "#A8FFFFFF"; border.width: 1; border.color: "#A8FFFFFF"
                Text {
                    anchors.centerIn: parent
                    text: pinyin.chineseMode && pinyin.composition.length > 0 ? "选择首词" : "空格"
                    color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textBody
                }
                TapHandler { onTapped: root.activate("SPACE") }
            }
            Rectangle {
                width: 174; height: 44; radius: 14
                gradient: Gradient {
                    GradientStop { position: 0; color: "#D0319CFF" }
                    GradientStop { position: 1; color: "#ED0668E4" }
                }
                Text {
                    anchors.centerIn: parent; text: "完成"; color: "white"
                    font.family: Theme.fontFamily; font.pixelSize: Theme.textLabel; font.weight: Font.DemiBold
                }
                TapHandler { onTapped: root.finishInput() }
            }
        }
    }
}
