import QtQuick 2.15
import QtQuick.Layouts 1.15
import PressureOS 1.0
import "../components"

Item {
    id: root
    property string pendingDeleteId: ""
    property string pendingDeleteName: ""
    property string pendingDeleteKind: "task"
    property int pendingDeleteSessionId: -1
    property var combinedEntries: buildEntries(tasks.taskList, database.measurementSessions)
    readonly property bool compactLayout: width < 1160 || height < 600
    readonly property bool modalActive: createDialog.opened || quickDialog.opened
                                        || deleteConfirm.visible || recordDialog.opened

    Item {
        id: pageContent
        anchors.fill: parent
        enabled: !root.modalActive

        ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compactLayout ? 18 : 32
        anchors.rightMargin: root.compactLayout ? 18 : 32
        anchors.topMargin: 2
        anchors.bottomMargin: 4
        spacing: root.compactLayout ? 8 : 12

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactLayout ? 62 : 66

            ColumnLayout {
                spacing: 0
                Text {
                    text: "TASK-CENTRIC WORKSPACE"
                    color: Theme.blue
                    font.family: Theme.numberFont
                    font.pixelSize: Theme.textMicro
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.15
                }
                Text {
                    text: "我的任务"
                    color: Theme.inkStrong
                    font.family: Theme.fontFamily
                    font.pixelSize: root.compactLayout ? 25 : 27
                    font.weight: Font.Bold
                }
                Text {
                    Layout.maximumWidth: root.compactLayout ? 560 : 780
                    text: "模板、测量、分析和导出收拢在同一个任务里，最近编辑的任务排在最上方。"
                    color: Theme.inkMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textMicro
                    elide: Text.ElideRight
                }
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: 8
                PremiumButton {
                    Layout.preferredWidth: root.compactLayout ? 158 : 168
                    Layout.preferredHeight: 46
                    compact: true
                    horizontalPadding: 12
                    text: "快速空白任务"
                    iconName: "spark"
                    variant: "secondary"
                    onClicked: quickDialog.opened = true
                }
                PremiumButton {
                    Layout.preferredWidth: root.compactLayout ? 174 : 184
                    Layout.preferredHeight: 46
                    compact: true
                    horizontalPadding: 13
                    text: "从模板创建"
                    iconName: "play"
                    variant: "primary"
                    onClicked: createDialog.opened = true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: root.compactLayout ? 58 : 74
            spacing: root.compactLayout ? 8 : 10

            Repeater {
                model: [
                    { label: "任务与记录", value: root.combinedEntries.length, color: Theme.blue, icon: "task" },
                    { label: "待测数据", value: root.countStatus("待测数据"), color: "#1683FF", icon: "record" },
                    { label: "待分析 / 导出", value: root.countPending(), color: Theme.orange, icon: "chart" },
                    { label: "已完成 / 保存", value: root.countStatus("已完成") + root.countStatus("已保存"), color: Theme.green, icon: "check" }
                ]
                delegate: GlassPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: root.compactLayout ? 16 : 18
                    elevated: false

                    Item {
                        anchors.fill: parent
                        anchors.margins: root.compactLayout ? 9 : 13
                        Rectangle {
                            width: root.compactLayout ? 34 : 36
                            height: width
                            radius: 11
                            color: Qt.rgba(Qt.color(modelData.color).r,
                                           Qt.color(modelData.color).g,
                                           Qt.color(modelData.color).b, 0.11)
                            VectorIcon {
                                anchors.centerIn: parent
                                width: 17
                                height: 17
                                name: modelData.icon
                                color: modelData.color
                            }
                        }
                        Text {
                            x: root.compactLayout ? 44 : 48
                            y: root.compactLayout ? -2 : 0
                            text: modelData.value
                            color: Theme.inkStrong
                            font.family: Theme.numberFont
                            font.pixelSize: root.compactLayout ? 21 : 22
                            font.weight: Font.Bold
                        }
                        Text {
                            x: root.compactLayout ? 44 : 48
                            y: root.compactLayout ? 25 : 29
                            text: modelData.label
                            color: Theme.inkMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMicro
                            font.weight: Font.Medium
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: root.compactLayout ? 10 : 14

            GlassPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: root.compactLayout ? 20 : 24
                clip: true

                Item {
                    anchors.fill: parent
                    anchors.margins: root.compactLayout ? 14 : 19

                    Row {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        spacing: 9
                        Text {
                            text: "最近任务与记录"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: root.compactLayout ? 15 : 16
                            font.weight: Font.Bold
                        }
                        StatusChip {
                            height: 25
                            text: "按最后编辑时间"
                            accent: Theme.blue
                            dot: false
                        }
                    }

                    Text {
                        visible: !root.compactLayout
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: 3
                            text: "任务进入引导流程；自由测量记录可直接查看全程波形"
                        color: Theme.inkMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textMicro
                    }

                    ListView {
                        id: taskList
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: root.compactLayout ? 34 : 38
                        anchors.bottom: parent.bottom
                        clip: true
                        spacing: root.compactLayout ? 8 : 9
                        model: root.combinedEntries
                        boundsBehavior: Flickable.DragOverBounds
                        boundsMovement: Flickable.FollowBoundsBehavior
                        flickDeceleration: 2200
                        maximumFlickVelocity: 4200
                        pressDelay: 80

                        delegate: Rectangle {
                            id: taskCard
                            objectName: "taskCard_" + index
                            width: taskList.width
                            height: root.compactLayout ? 76 : 93
                            radius: root.compactLayout ? 16 : 18
                            color: hover.hovered ? "#EEF7FD" : "#F8FBFE"
                            border.width: 1
                            border.color: hover.hovered ? "#A9D6F6" : Theme.line

                            Rectangle {
                                x: 12
                                y: root.compactLayout ? 11 : 14
                                width: root.compactLayout ? 42 : 46
                                height: width
                                radius: 13
                                color: modelData.accent
                                opacity: 0.96
                                VectorIcon {
                                    anchors.centerIn: parent
                                    width: 21
                                    height: 21
                                    name: modelData.icon
                                    color: "white"
                                }
                            }

                            Text {
                                x: root.compactLayout ? 64 : 72
                                y: root.compactLayout ? 9 : 13
                                width: taskCard.width - x - (root.compactLayout ? 184 : 210)
                                text: modelData.name
                                color: Theme.ink
                                font.family: Theme.fontFamily
                                font.pixelSize: root.compactLayout ? 13 : 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                x: root.compactLayout ? 64 : 72
                                y: root.compactLayout ? 33 : 39
                                width: taskCard.width - x - (root.compactLayout ? 166 : 190)
                                text: modelData.templateName + " · " + modelData.detail
                                color: Theme.inkMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.textMicro
                                elide: Text.ElideRight
                            }
                            Rectangle {
                                x: root.compactLayout ? 64 : 72
                                y: root.compactLayout ? 58 : 67
                                width: taskCard.width - x - (root.compactLayout ? 164 : 198)
                                height: 5
                                radius: 3
                                color: "#DDEAF3"
                                Rectangle {
                                    width: parent.width * modelData.progress
                                    height: parent.height
                                    radius: parent.radius
                                    color: modelData.accent
                                }
                            }

                            StatusChip {
                                anchors.right: deleteButton.left
                                anchors.rightMargin: 10
                                y: root.compactLayout ? 10 : 14
                                height: 27
                                text: modelData.status
                                accent: modelData.statusColor
                                dot: true
                            }
                            Text {
                                anchors.right: deleteButton.left
                                anchors.rightMargin: 10
                                y: root.compactLayout ? 43 : 54
                                text: "最后编辑 " + modelData.updated
                                color: Theme.inkMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: root.compactLayout ? 9 : Theme.textMicro
                            }
                            Rectangle {
                                id: deleteButton
                                objectName: "taskDeleteButton_" + index
                                z: 2
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                y: root.compactLayout ? 18 : 25
                                width: 48
                                height: 48
                                radius: 15
                                color: deleteHover.hovered ? "#FFF0F2" : "#EAF2F8"
                                DeleteIcon {
                                    anchors.centerIn: parent
                                    width: 18
                                    height: 18
                                    color: deleteHover.hovered ? Theme.red : Theme.inkMuted
                                }
                                HoverHandler { id: deleteHover }
                                TapHandler {
                                    onTapped: {
                                         root.pendingDeleteId = modelData.id
                                         root.pendingDeleteName = modelData.name
                                         root.pendingDeleteKind = modelData.kind || "task"
                                         root.pendingDeleteSessionId = modelData.sessionId || -1
                                         deleteConfirm.visible = true
                                    }
                                }
                            }
                            HoverHandler { id: hover }
                            Item {
                                // The navigation target deliberately ends before the
                                // delete button. Non-overlapping hit areas avoid touch
                                // event propagation selecting the task during deletion.
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.right: deleteButton.left
                                anchors.rightMargin: 4
                                z: 1
                                TapHandler {
                                    acceptedButtons: Qt.LeftButton
                                    gesturePolicy: TapHandler.ReleaseWithinBounds
                                    onTapped: {
                                        if (modelData.kind === "recording") {
                                            recordDialog.sessionId = modelData.sessionId
                                            recordDialog.opened = true
                                        } else if (tasks.selectTask(modelData.id)) {
                                            app.navigate("runner")
                                        }
                                    }
                                }
                            }
                            Behavior on color { ColorAnimation { duration: 140 } }
                        }

                        Text {
                            visible: taskList.count === 0
                            anchors.centerIn: parent
                            text: "还没有任务或测量记录\n从模板创建第一项任务吧"
                            horizontalAlignment: Text.AlignHCenter
                            color: Theme.inkMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            lineHeight: 1.5
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: root.compactLayout ? 280 : 310
                Layout.minimumWidth: root.compactLayout ? 280 : 310
                Layout.maximumWidth: root.compactLayout ? 280 : 310
                Layout.fillHeight: true
                spacing: root.compactLayout ? 9 : 11

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.compactLayout ? 180 : 226
                    radius: root.compactLayout ? 20 : 22
                    clip: true

                    Item {
                        anchors.fill: parent
                        anchors.margins: root.compactLayout ? 14 : 18
                        Text {
                            text: "一条清晰的完成路径"
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: 14
                            font.weight: Font.Bold
                        }
                        Text {
                            y: 23
                            text: "任务会记住你做到哪一步"
                            color: Theme.inkMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.textMicro
                        }
                        Column {
                            y: root.compactLayout ? 43 : 47
                            width: parent.width
                            spacing: root.compactLayout ? 1 : 2
                            Repeater {
                                model: [
                                    { number: "01", title: "了解任务", detail: "目标、流程与安全要求" },
                                    { number: "02", title: "逐点测量", detail: "稳定采集并随时纠错" },
                                    { number: "03", title: "处理与分析", detail: "拟合、残差和不确定度" },
                                    { number: "04", title: "导出归档", detail: "数据、图表和分析摘要" }
                                ]
                                delegate: Row {
                                    width: parent.width
                                    height: root.compactLayout ? 26 : 35
                                    spacing: 9
                                    Rectangle {
                                        width: root.compactLayout ? 24 : 27
                                        height: width
                                        radius: 8
                                        color: index === 0 ? Theme.blue : "#E7F0F7"
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.number
                                            color: index === 0 ? "white" : Theme.inkMuted
                                            font.family: Theme.numberFont
                                            font.pixelSize: Theme.textSmall
                                            font.weight: Font.Bold
                                        }
                                    }
                                    Column {
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 0
                                        Text {
                                            width: parent.parent.width - parent.parent.spacing
                                                   - (root.compactLayout ? 24 : 27)
                                            text: root.compactLayout
                                                  ? modelData.title + " · " + modelData.detail
                                                  : modelData.title
                                            color: Theme.ink
                                            font.family: Theme.fontFamily
                                            font.pixelSize: root.compactLayout
                                                            ? Theme.textMicro : Theme.textSmall
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            visible: !root.compactLayout
                                            text: modelData.detail
                                            color: Theme.inkMuted
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.textSmall
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                GlassPanel {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: root.compactLayout ? 20 : 22
                    elevated: false
                    panelColor: "#F0F9FFFF"
                    panelColorBottom: "#E8F4FBFF"
                    clip: true

                    Item {
                        anchors.fill: parent
                        anchors.margins: root.compactLayout ? 14 : 18
                        Row {
                            spacing: 8
                            VectorIcon { width: 18; height: 18; name: "spark"; color: Theme.blue }
                            Text {
                                text: root.compactLayout ? "任务为什么不会丢？" : "为什么采用任务胶囊？"
                                color: Theme.blueDeep
                                font.family: Theme.fontFamily
                                font.pixelSize: root.compactLayout ? 12 : 13
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            y: 30
                            width: parent.width
                            height: root.compactLayout ? 38 : implicitHeight
                            text: root.compactLayout
                                  ? "每次采点立即保存；退出、切换任务或重启设备都不会丢失数据。"
                                  : "退出页面、切换任务或设备重启都不会丢失已采数据。每个任务独立保存模板版本、变量来源、处理方法和最终产物。"
                            wrapMode: Text.WordWrap
                            maximumLineCount: root.compactLayout ? 2 : 4
                            elide: Text.ElideRight
                            color: Theme.ink
                            font.family: Theme.fontFamily
                            font.pixelSize: root.compactLayout ? Theme.textMicro : Theme.textSmall
                            lineHeight: 1.35
                        }
                        Rectangle {
                            visible: !root.compactLayout
                            y: 92
                            width: parent.width
                            height: 1
                            color: Theme.line
                        }
                        Column {
                            visible: !root.compactLayout
                            y: 105
                            width: parent.width
                            spacing: 7
                            Repeater {
                                model: [
                                    "每次采点立即写入 SQLite WAL",
                                    "删除误测点后自动重算结果",
                                    "多个任务可在侧栏无缝切换",
                                    "进度和最后编辑时间持续更新"
                                ]
                                delegate: Row {
                                    spacing: 7
                                    Rectangle {
                                        width: 18
                                        height: 18
                                        radius: 6
                                        color: Theme.greenSoft
                                        VectorIcon { anchors.centerIn: parent; width: 10; height: 10; name: "check"; color: Theme.green }
                                    }
                                    Text {
                                        text: modelData
                                        color: Theme.inkMuted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.textMicro
                                    }
                                }
                            }
                        }
                        PremiumButton {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 42
                            compact: true
                            text: root.compactLayout ? "新建任务" : "创建新的任务胶囊"
                            iconName: "play"
                            variant: "secondary"
                            onClicked: createDialog.opened = true
                        }
                    }
                }
            }
        }
        }
    }

    function buildEntries(taskItems, sessionItems) {
        let merged = []
        for (let i = 0; i < taskItems.length; ++i)
            merged.push(taskItems[i])
        for (let j = 0; j < sessionItems.length; ++j)
            merged.push(sessionItems[j])
        merged.sort(function(left, right) {
            return Number(right.updatedAt || 0) - Number(left.updatedAt || 0)
        })
        return merged
    }

    function countStatus(status) {
        let count = 0
        for (let i = 0; i < root.combinedEntries.length; ++i)
            if (root.combinedEntries[i].status === status) ++count
        return count
    }

    function countPending() {
        return countStatus("待数据分析") + countStatus("待导出结果")
    }

    CreateTaskDialog {
        id: createDialog
        anchors.fill: parent
        onTaskCreated: app.navigate("runner")
    }

    QuickTaskDialog {
        id: quickDialog
        anchors.fill: parent
        onTaskCreated: app.navigate("runner")
    }

    MeasurementRecordDialog {
        id: recordDialog
        anchors.fill: parent
    }

    Rectangle {
        id: deleteConfirm
        visible: false
        anchors.fill: parent
        color: Theme.modalScrim
        z: 140

        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons }

        GlassPanel {
            anchors.centerIn: parent
            width: Math.min(430, root.width - 40)
            height: 220
            radius: 25
            material: "modal"
            tintStrength: 0.012
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; preventStealing: true }
            Item {
                anchors.fill: parent
                anchors.margins: 23
                Rectangle {
                    width: 42; height: 42; radius: 13; color: "#FFF0F2"
                    VectorIcon { anchors.centerIn: parent; width: 20; height: 20; name: "delete"; color: Theme.red }
                }
                Text {
                    x: 55; y: 1
                    text: root.pendingDeleteKind === "recording" ? "删除这段自由测量记录？" : "删除这个任务？"
                    color: Theme.inkStrong
                    font.family: Theme.fontFamily; font.pixelSize: 17; font.weight: Font.Bold
                }
                Text {
                    x: 55; y: 27; width: parent.width - 55; text: root.pendingDeleteName
                    color: Theme.inkMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.textSmall
                    elide: Text.ElideRight
                }
                Text {
                    y: 65; width: parent.width
                    text: root.pendingDeleteKind === "recording"
                          ? "整段波形与全部采样点会从本机数据仓库中删除，此操作不可撤销。"
                          : "任务中的测量点和本地分析记录会一并删除，此操作不可撤销。"
                    wrapMode: Text.WordWrap; color: Theme.ink; font.family: Theme.fontFamily
                    font.pixelSize: Theme.textBody; lineHeight: 1.35
                }
                Row {
                    anchors.right: parent.right; anchors.bottom: parent.bottom; spacing: 9
                    PremiumButton { width: 96; height: 48; compact: true; text: "取消"; variant: "ghost"; onClicked: deleteConfirm.visible = false }
                    PremiumButton {
                        width: 128; height: 48; compact: true; text: "确认删除"; iconName: "delete"; variant: "danger"
                        onClicked: {
                            if (root.pendingDeleteKind === "recording") {
                                if (database.deleteMeasurementSession(root.pendingDeleteSessionId))
                                    app.showToast("自由测量记录已删除")
                                else
                                    app.showToast("记录删除失败，请确认当前未在采集")
                            } else {
                                tasks.deleteTask(root.pendingDeleteId)
                            }
                            deleteConfirm.visible = false
                        }
                    }
                }
            }
        }
    }
}
