import QtQuick 2.15
import QtQuick.Layouts 1.15
import PressureOS 1.0
import "../components"

Item {
    id: root
    signal waveformFocusRequested()

    readonly property bool compactLayout: width < 1160 || height < 600
    readonly property bool modalActive: zeroGuide.opened
    readonly property color safetyAccent: device.safetyLevel === "trip" ? Theme.red
                                                   : (device.safetyLevel === "warning" ? "#EF7A42"
                                                   : (device.safetyLevel === "caution" ? Theme.orange
                                                                                       : Theme.green))
    readonly property color safetySoft: device.safetyLevel === "trip" ? "#FFF0F3"
                                                 : (device.safetyLevel === "warning" ? "#FFF1E8"
                                                 : (device.safetyLevel === "caution" ? Theme.orangeSoft
                                                                                     : Theme.greenSoft))
    readonly property bool safetyAttention: device.safetyLevel === "caution"
                                               || device.safetyLevel === "warning"
                                               || device.safetyLevel === "trip"
    readonly property real chartRate: device.sampleRate > 0
                                          ? device.sampleRate
                                          : (device.hardwareMode ? 10 : 50)
    readonly property int mainChartPoints: Math.max(60, Math.round(chartRate * 15))
    readonly property string trustText: !device.hardwareMode ? "模拟数据"
                                        : (!deviceLink.dataFresh ? "数据超时"
                                           : (deviceLink.protocolIntegrityAvailable
                                              ? "实时 · CRC 已校验"
                                              : "实时 · V0 兼容"))

    ColumnLayout {
        anchors.fill: parent
        enabled: !zeroGuide.opened
        anchors.leftMargin: root.compactLayout ? 18 : 32
        anchors.rightMargin: root.compactLayout ? 18 : 32
        anchors.topMargin: 3
        anchors.bottomMargin: 4
        spacing: root.compactLayout ? 8 : 10

        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 50
            GlassPanel {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                Layout.minimumWidth: 48
                Layout.maximumWidth: 48
                Layout.minimumHeight: 48
                Layout.maximumHeight: 48
                radius: 16; material: "clear"; elevated: true; interactive: true
                tintStrength: 0.055
                accessibleName: "返回桌面"
                VectorIcon { anchors.centerIn: parent; width: 17; height: 17; name: "back"; color: Theme.inkMuted }
                onClicked: app.navigate("home")
            }
            ColumnLayout {
                Layout.preferredWidth: 178
                Layout.minimumWidth: 154
                spacing: 0
                Text { text: "PressureOS  /  自由测量"; color: Theme.inkFaint; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro }
                Text { text: "实时测量"; color: Theme.inkStrong; font.family: Theme.fontFamily; font.pixelSize: Theme.textTitle; font.weight: Font.Bold }
            }
            Item { Layout.fillWidth:true }
            StatusChip {
                text: device.recording ? "正在记录  "+device.recordSeconds+" s"
                      : (device.hardwareMode
                         ? (deviceLink.dataFresh ? device.sampleRate+" Hz · 下位机" : deviceLink.statusText)
                         : "50 Hz · 模拟")
                accent: device.recording ? Theme.red
                        : (device.hardwareMode && !deviceLink.dataFresh ? Theme.orange : Theme.green)
                iconName:device.recording?"record":"pulse"
            }
            StatusChip { text:"工作量程  "+device.rangeText;accent:root.safetyAccent;iconName:"shield" }
        }

        GlassPanel {
            id: safetyStrip
            visible: root.safetyAttention
            Layout.fillWidth:true
            Layout.preferredHeight: root.compactLayout ? 54 : 56
            radius:16
            material:"regular"
            tint:root.safetyAccent
            tintStrength:root.safetyAttention ? 0.085 : 0.045
            elevated:false
            borderColor:root.safetyAttention ? Qt.lighter(root.safetyAccent,1.55) : Theme.glassBorder
            Rectangle{anchors.fill:parent;radius:safetyStrip.radius;color:root.safetySoft;opacity:root.safetyAttention?0.42:0.10}
            Row { anchors.left:parent.left;anchors.leftMargin:15;anchors.verticalCenter:parent.verticalCenter;spacing:11
                Rectangle{width:root.safetyAttention?32:26;height:width;radius:root.safetyAttention?10:9;color:root.safetyAccent;VectorIcon{anchors.centerIn:parent;width:root.safetyAttention?17:14;height:width;name:device.safetyLevel==="normal"?"shield":"warning";color:"white"}}
                Column{anchors.verticalCenter:parent.verticalCenter;spacing:1;Text{text:device.safetyTitle;color:device.safetyLevel==="trip"?"#A92E48":Theme.ink;font.family:Theme.fontFamily;font.pixelSize:11;font.weight:Font.DemiBold}Text{visible:root.safetyAttention;text:device.safetyMessage;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;elide:Text.ElideRight;width:root.width-390}}
            }
            Row { anchors.right:parent.right;anchors.rightMargin:14;anchors.verticalCenter:parent.verticalCenter;spacing:9
                Text{text:"量程使用 "+device.utilizationPercent.toFixed(1)+"%";color:root.safetyAccent;font.family:Theme.numberFont;font.pixelSize:11;font.weight:Font.DemiBold}
                PremiumButton{visible:device.tripLatched;width:112;height:48;compact:true;text:device.utilizationPercent>75?"人工泄压":"人工复位";iconName:device.utilizationPercent>75?"trend":"check";variant:"danger";onClicked:device.utilizationPercent>75?device.simulateVentToAtmosphere():device.acknowledgeTrip()}
            }
        }

        RowLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;spacing:root.compactLayout ? 10 : 14
            GlassPanel {
                Layout.fillWidth:true;Layout.fillHeight:true;radius:24;material:"dense";tint:Theme.blue;tintStrength:0.045
                Item { anchors.fill:parent;anchors.margins:root.compactLayout ? 14 : 21
                    Row { anchors.left:parent.left;anchors.top:parent.top;spacing:8
                        Rectangle{width:8;height:8;radius:4;color:device.stable?Theme.green:Theme.orange;anchors.verticalCenter:parent.verticalCenter}
                        Text{text:"当前示值";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize:11;font.weight:Font.DemiBold}
                    }
                    Row {
                        id:readingRow;anchors.left:parent.left;anchors.top:parent.top;anchors.topMargin:25;spacing:12
                        Text { text:device.formattedPressure;color:Theme.inkStrong;font.family:Theme.numberFont;font.pixelSize:root.compactLayout ? (text.length>8?46:58) : (text.length>8?54:70);font.weight:Font.DemiBold;font.letterSpacing:-1.6 }
                        PremiumComboBox { width:118;height:48;anchors.bottom:parent.bottom;anchors.bottomMargin:root.compactLayout ? 2 : 8;label:"显示单位";iconName:"gauge";model:device.unitOptions;currentValue:device.unit;onSelected:function(value){device.setUnit(value)} }
                    }
                    Column { anchors.right:parent.right;anchors.top:parent.top;spacing:5
                        StatusChip{text:root.trustText;accent:!device.hardwareMode?Theme.orange:(!deviceLink.dataFresh?Theme.red:(deviceLink.protocolIntegrityAvailable?Theme.green:Theme.orange));iconName:deviceLink.dataFresh||!device.hardwareMode?"pulse":"warning"}
                        Text{anchors.right:parent.right;text:"峰峰值 "+device.stabilityP2P.toFixed(3)+" kPa";color:Theme.inkFaint;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro}
                    }
                    Row { anchors.left:parent.left;anchors.top:parent.top;anchors.topMargin:root.compactLayout ? 88 : 105;spacing:12
                        Text{text:"原始值  "+device.formattedRawPressure+" "+device.unit;color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                        Rectangle{width:1;height:11;color:Theme.line}
                        Text{text:"零点偏移  "+(device.zeroOffsetKPa>=0?"+":"")+device.zeroOffsetKPa.toFixed(4)+" kPa";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                        Rectangle{width:1;height:11;color:Theme.line}
                        Text{text:"变化率  "+(device.pressureRateKPaPerSec>=0?"+":"")+device.pressureRateKPaPerSec.toFixed(2)+" kPa/s";color:Math.abs(device.pressureRateKPaPerSec)>2?Theme.orange:Theme.inkFaint;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro}
                    }

                    Item {
                        id: chartHeader
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.topMargin: root.compactLayout ? 106 : 126
                        height: 34

                        Row {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 9
                            Text{text:"实时趋势";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.DemiBold}
                            Text{text:"15 秒滚动 · 纵轴自适应 · 点击波形可全屏";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                        }

                        GlassPanel {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 108
                            height: 32
                            radius: 11
                            material: "clear"
                            elevated: false
                            interactive: true
                            accessibleName: "打开全屏波形"
                            onClicked: root.waveformFocusRequested()

                            Row {
                                anchors.centerIn: parent
                                spacing: 6
                                VectorIcon { width: 14; height: 14; name: "expand"; color: Theme.blue }
                                Text { text: "全屏波形"; color: Theme.blueDeep; font.family: Theme.fontFamily; font.pixelSize: Theme.textMicro; font.weight: Font.DemiBold }
                            }
                        }
                    }
                    RealtimeChart {
                        id: mainChart
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: chartHeader.bottom
                        anchors.topMargin: 2
                        anchors.bottom: controls.top
                        anchors.bottomMargin: 7
                        series: device.series
                        rawSeries: device.rawSeries
                        showRaw: true
                        showTimeLabels: true
                        showSafetyAxis: true
                        maxPoints: root.mainChartPoints
                        sampleRateHz: root.chartRate
                        resolution: device.resolutionKPa
                        rangeMinimum: device.rangeMinKPa
                        rangeMaximum: device.rangeMaxKPa
                        interactive: true
                        onActivated: root.waveformFocusRequested()
                    }
                    RowLayout { id:controls;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:root.compactLayout ? 50 : 55;spacing:9
                        PremiumButton{Layout.preferredWidth:162;Layout.fillHeight:true;text:"零点漂移校正";iconName:"zero";variant:"secondary";onClicked:zeroGuide.opened=true}
                        PremiumComboBox{Layout.preferredWidth:205;Layout.fillHeight:true;openUpward:true;label:"滤波方式";iconName:"filter";model:device.filterOptions;currentValue:device.filterName;onSelected:function(value){device.setFilter(value)}}
                        PremiumButton{Layout.fillWidth:true;Layout.fillHeight:true;text:device.recording?"停止并保存":"开始记录";iconName:device.recording?"check":"record";variant:"primary";accent:device.recording?Theme.red:Theme.blue;enabled:!device.tripLatched;onClicked:device.toggleRecording()}
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth:root.compactLayout ? 260 : 295
                Layout.minimumWidth:root.compactLayout ? 260 : 295
                Layout.maximumWidth:root.compactLayout ? 260 : 295
                Layout.fillHeight:true;spacing:root.compactLayout ? 8 : 10
                GlassPanel { Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout ? 126 : 145;radius:21;material:"regular";tint:root.safetyAccent;tintStrength:0.05
                    Item{anchors.fill:parent;anchors.margins:root.compactLayout ? 14 : 17
                        Row{spacing:8;VectorIcon{width:17;height:17;name:"shield";color:root.safetyAccent}Text{text:"安全监测";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.Bold}}
                        Text{
                            anchors.right:parent.right
                            text:!device.hardwareMode?"模拟监测":(!deviceLink.dataFresh?"数据超时":(root.safetyAttention?device.safetyTitle:"数据监测中"))
                            color:!deviceLink.dataFresh&&device.hardwareMode?Theme.red:root.safetyAccent
                            font.family:Theme.fontFamily;font.pixelSize:Theme.textMicro;font.weight:Font.DemiBold
                        }
                        Column{y:29;width:parent.width;spacing:5
                            Row{width:parent.width;height:18;spacing:7
                                Rectangle{width:15;height:15;radius:5;color:Theme.greenSoft;VectorIcon{anchors.centerIn:parent;width:9;height:9;name:"check";color:Theme.green}}
                                Text{width:parent.width-22;text:"80% 提醒 · 90% 警告 · 98% 高风险";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize:Theme.textMicro;elide:Text.ElideRight}
                            }
                            Row{width:parent.width;height:18;spacing:7
                                Rectangle{width:15;height:15;radius:5;color:Theme.greenSoft;VectorIcon{anchors.centerIn:parent;width:9;height:9;name:"check";color:Theme.green}}
                                Text{width:parent.width-22;text:"趋势预测 · 断线与采样超时";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize:Theme.textMicro;elide:Text.ElideRight}
                            }
                            Row{width:parent.width;height:18;spacing:7
                                Rectangle{width:15;height:15;radius:5;color:deviceLink.protocolIntegrityAvailable?Theme.greenSoft:Theme.orangeSoft;VectorIcon{anchors.centerIn:parent;width:9;height:9;name:deviceLink.protocolIntegrityAvailable?"check":"info";color:deviceLink.protocolIntegrityAvailable?Theme.green:Theme.orange}}
                                Text{width:parent.width-22;text:deviceLink.protocolIntegrityAvailable?"V1 协议 · CRC 与帧序号已校验":"等待 V1 协议完整性数据";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize:Theme.textMicro;elide:Text.ElideRight}
                            }
                        }
                    }
                }
                GlassPanel { Layout.fillWidth:true;Layout.fillHeight:root.compactLayout;Layout.preferredHeight:root.compactLayout ? -1 : 171;radius:21;material:"regular";tint:Theme.cyan;tintStrength:0.045
                    Item{anchors.fill:parent;anchors.margins:root.compactLayout ? 14 : 17
                        Text{text:"测量状态";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.Bold}
                        Column{y:31;width:parent.width;spacing:root.compactLayout ? 9 : 6
                            MeasurementStatusItem{
                                width:parent.width;iconName:"thermometer";accent:"#F29A43"
                                label:device.hardwareMode&&!deviceLink.temperatureChannelEnabled?"采样模式":"环境温度"
                                value:device.hardwareMode&&!deviceLink.temperatureChannelEnabled?"压力单通道高速":device.temperature.toFixed(1)+" °C"
                            }
                            MeasurementStatusItem{
                                width:parent.width;iconName:"pulse";accent:"#19B987";label:"短时波动"
                                value:"±"+(device.stabilityP2P/2).toFixed(3)+" kPa"
                            }
                            MeasurementStatusItem{
                                width:parent.width;iconName:"clock";accent:"#5479F4";label:"采样与数据时效"
                                value:(device.sampleRate>0?device.sampleRate+" Hz":"—")+" · "+(device.hardwareMode?(deviceLink.lastFrameAgeMs>=0?deviceLink.lastFrameAgeMs+" ms":"等待数据"):"20 ms")
                            }
                        }
                    }
                }
                GlassPanel { visible:!root.compactLayout;Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:150;radius:21;material:"regular";tint:Theme.violet;tintStrength:0.055
                    Item{anchors.fill:parent;anchors.margins:17
                        Text{text:"设备与证据链";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.Bold}
                        Row{y:31;spacing:10;Rectangle{width:38;height:38;radius:12;gradient:Gradient{GradientStop{position:0;color:"#5EB3FF"}GradientStop{position:1;color:"#2378E7"}} VectorIcon{anchors.centerIn:parent;width:19;height:19;name:"device";color:"white"}}Column{anchors.verticalCenter:parent.verticalCenter;spacing:2;Text{text:device.hardwareMode?(deviceLink.deviceId!==""?deviceLink.deviceId:"STM32 压力模块"):"PressureOS 模拟数据源";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textBody;font.weight:Font.DemiBold}Text{text:device.hardwareMode?("AD7124-8 · "+deviceLink.protocolName):"交互验证 · 非计量数据";color:Theme.inkFaint;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro}}}
                        Column{y:77;width:parent.width;spacing:4
                            Repeater{model:[
                                {l:"固件版本",v:device.hardwareMode?(deviceLink.firmwareVersion!==""?deviceLink.firmwareVersion:"等待 INFO"):"Simulation",c:Theme.violet},
                                {l:"协议完整性",v:device.hardwareMode?(deviceLink.protocolIntegrityAvailable?"CRC + 序号已校验":"等待下位机 V1"):"不适用",c:deviceLink.protocolIntegrityAvailable?Theme.green:Theme.orange},
                                {l:"数据库",v:database.ready?"WAL · 正常":"异常",c:database.ready?Theme.green:Theme.red}
                            ];delegate:Item{width:parent.width;height:13;Text{anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter;text:modelData.l;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;text:modelData.v;color:modelData.c;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro;font.weight:Font.DemiBold}}}
                        }
                        PremiumButton{anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:48;compact:true;text:"打开设备诊断";iconName:"device";variant:"secondary";onClicked:app.navigate("device")}
                    }
                }
            }
        }
    }
    ZeroCalibrationDialog { id:zeroGuide;anchors.fill:parent }
    Component.onCompleted: {
        if (launchZeroCalibration)
            zeroGuide.opened = true
    }
}
