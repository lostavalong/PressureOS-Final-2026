import QtQuick 2.15
import PressureOS 1.0

GlassPanel {
    id: root
    property string label: "指标"
    property string value: "--"
    property string unit: ""
    property string hint: ""
    property string iconName: "pulse"
    property color accent: Theme.blue
    property bool textValue: false
    property color valueColor: textValue ? accent : Theme.inkStrong
    property string valueFontFamily: textValue ? Theme.fontFamily : Theme.numberFont
    property int valuePixelSize: textValue
                                 ? (ultraDense ? 12 : (dense ? 14 : 16))
                                 : (ultraDense ? 15 : (dense ? 18 : 22))
    readonly property bool ultraDense: height < 56
    readonly property bool dense: height <= 100
    implicitWidth: 190
    implicitHeight: 104
    radius: 18
    elevated: false
    contentPadding: ultraDense ? 5 : (dense ? 8 : 16)

    Row {
        anchors.left: parent.left
        anchors.top: parent.top
        spacing: 8
        Rectangle {
            visible: !root.dense
            width: 28; height: 28; radius: 9
            color: Qt.rgba(root.accent.r, root.accent.g, root.accent.b, 0.10)
            VectorIcon { anchors.centerIn: parent; width: 15; height: 15; name: root.iconName; color: root.accent }
        }
        Text {
            id: labelText
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, root.width - root.contentPadding * 2
                               - (hintText.visible ? hintText.implicitWidth + 9 : 0))
            text: root.label
            color: Theme.inkMuted
            font.family: Theme.fontFamily
            font.pixelSize: root.ultraDense ? 9
                                             : (root.dense ? Theme.textSmall : Theme.textBody)
            maximumLineCount: 1
            elide: Text.ElideRight
        }
    }
    Text {
        id: valueText
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.ultraDense ? 18 : (root.dense ? 23 : 30)
        text: root.value + (root.unit !== "" ? "  " + root.unit : "")
        color: root.valueColor
        font.family: root.valueFontFamily
        font.pixelSize: root.valuePixelSize
        font.weight: Font.DemiBold
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: root.ultraDense ? 10 : (root.dense ? 11 : 13)
        verticalAlignment: Text.AlignBottom
        elide: Text.ElideRight
    }
    Text {
        id: hintText
        visible: root.hint !== "" && !root.ultraDense
        anchors.right: parent.right
        y: root.dense ? 0 : parent.height-height
        text: root.hint
        color: root.accent
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textMicro
        font.weight: Font.DemiBold
        maximumLineCount: 1
        elide: Text.ElideRight
    }
}
