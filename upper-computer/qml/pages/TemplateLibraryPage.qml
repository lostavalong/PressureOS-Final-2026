import QtQuick 2.15
import QtQuick.Dialogs
import QtQuick.Layouts 1.15
import PressureOS 1.0
import "../components"

Item {
    id: root
    property int selectedIndex: 0
    readonly property bool compactLayout: width < 1160 || height < 600

    FileDialog {
        id: templateDialog
        title: "导入 PressureOS 任务模板"
        nameFilters: ["PressureOS 模板 (*.json)", "所有文件 (*)"]
        onAccepted: templateRepo.importTemplate(selectedFile)
    }

    ColumnLayout {
        anchors.fill:parent
        anchors.leftMargin:root.compactLayout?20:32;anchors.rightMargin:root.compactLayout?20:32;anchors.topMargin:3;anchors.bottomMargin:4
        spacing:root.compactLayout?8:14
        RowLayout {
            Layout.fillWidth:true
            Layout.preferredHeight:root.compactLayout?64:66
            Layout.minimumHeight:Layout.preferredHeight
            Layout.maximumHeight:Layout.preferredHeight
            ColumnLayout {
                Layout.fillWidth:true
                spacing:0
                Text{text:"把复杂实验变成可复用的操作路径";color:Theme.violet;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold}
                Text{text:"任务模板库";color:Theme.inkStrong;font.family:Theme.fontFamily;font.pixelSize:root.compactLayout?23:25;font.weight:Font.Bold}
                Text{Layout.fillWidth:true;text:templateRepo.templateCount+" 个模板可用 · 测量、输入、计算和报告由同一份定义驱动";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textBody;maximumLineCount:1;elide:Text.ElideRight}
            }
            PremiumButton{width:148;height:48;compact:true;text:"导入 JSON 模板";iconName:"export";variant:"primary";accent:Theme.violet;onClicked:templateDialog.open()}
        }

        RowLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;spacing:root.compactLayout?10:15
            GridLayout {
                Layout.fillWidth:true;Layout.fillHeight:true;columns:2;columnSpacing:root.compactLayout?9:12;rowSpacing:root.compactLayout?9:12
                Repeater {
                    model: templateRepo.templates
                    delegate: GlassPanel {
                        Layout.fillWidth:true;Layout.fillHeight:true;radius:22;interactive:true
                        panelColor: root.selectedIndex===index?"#F6FBFFFF":"#EDFFFFFF"
                        borderColor: root.selectedIndex===index?"#9FCFFF":"#D8FFFFFF"
                        onClicked: root.selectedIndex=index
                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:18
                            Rectangle{width:root.compactLayout?34:42;height:width;radius:11;color:"#EFF6FC";VectorIcon{anchors.centerIn:parent;width:root.compactLayout?18:21;height:width;name:modelData.icon;color:modelData.accent}}
                            StatusChip{anchors.right:parent.right;height:25;text:modelData.badge;accent:modelData.badge.indexOf("推荐")>=0?Theme.blue:(modelData.badge.indexOf("试验")>=0?Theme.orange:Theme.inkMuted);dot:false}
                            Text{y:root.compactLayout?43:54;width:parent.width;text:modelData.name;color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:root.compactLayout?13:15;font.weight:Font.Bold;elide:Text.ElideRight}
                            Text{y:root.compactLayout?64:80;text:modelData.category;color:modelData.accent;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold}
                            Text{y:root.compactLayout?83:102;width:parent.width;height:root.compactLayout?36:42;text:modelData.description;wrapMode:Text.WordWrap;maximumLineCount:2;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;lineHeight:1.25;elide:Text.ElideRight}
                            Rectangle{anchors.left:parent.left;anchors.right:parent.right;y:root.compactLayout?124:151;height:1;color:Theme.lineSoft}
                            Row{anchors.left:parent.left;anchors.bottom:parent.bottom;spacing:root.compactLayout?9:14
                                Row{spacing:5;VectorIcon{width:14;height:14;name:"clock";color:Theme.inkFaint}Text{text:modelData.duration;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}}
                                Row{spacing:5;VectorIcon{width:14;height:14;name:"file";color:Theme.inkFaint}Text{width:root.compactLayout?125:implicitWidth;text:modelData.outputs;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;elide:Text.ElideRight}}
                            }
                            Rectangle{visible:root.selectedIndex===index;anchors.right:parent.right;anchors.bottom:parent.bottom;width:27;height:27;radius:9;color:Theme.blue;VectorIcon{anchors.centerIn:parent;width:14;height:14;name:"check";color:"white"}}
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth:root.compactLayout?320:330;Layout.minimumWidth:Layout.preferredWidth;Layout.maximumWidth:Layout.preferredWidth
                Layout.fillHeight:true;spacing:root.compactLayout?9:12
                GlassPanel {
                    Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?286:380;radius:23
                    Item{anchors.fill:parent;anchors.margins:root.compactLayout?15:20
                        Text{text:"模板详情";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.Bold}
                        StatusChip{anchors.right:parent.right;height:25;text:"Schema 1.0";accent:Theme.green;iconName:"shield"}
                        Rectangle{y:root.compactLayout?33:40;width:root.compactLayout?40:48;height:width;radius:13;gradient:Gradient{GradientStop{position:0;color:"#9A8AFB"}GradientStop{position:1;color:"#6954E8"}} VectorIcon{anchors.centerIn:parent;width:root.compactLayout?20:24;height:width;name:"flask";color:"white"}}
                        Text{y:root.compactLayout?80:98;width:parent.width;text:templateRepo.templates[root.selectedIndex] ? templateRepo.templates[root.selectedIndex].name : "—";color:Theme.inkStrong;font.family:Theme.fontFamily;font.pixelSize:root.compactLayout?15:17;font.weight:Font.Bold;wrapMode:Text.WordWrap}
                        Text{y:root.compactLayout?105:130;width:parent.width;text:root.compactLayout?"封装操作、变量、稳定判定、计算与输出。":"模板同时封装操作说明、变量护照、稳定条件、计算图与输出格式。";wrapMode:Text.WordWrap;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textBody;lineHeight:1.3}
                        Column{y:root.compactLayout?129:170;width:parent.width;spacing:root.compactLayout?2:5
                            Repeater{model:[{n:"01",t:"检查装置并归零"},{n:"02",t:"逐点输入与稳定采集"},{n:"03",t:"统计、拟合与不确定度"},{n:"04",t:"审核并生成图表报告"}];delegate:Row{width:parent.width;height:root.compactLayout?18:implicitHeight;spacing:9
                                Rectangle{width:root.compactLayout?18:22;height:width;radius:6;color:index<2?Theme.blueSoft:"#F0F3F7";Text{anchors.centerIn:parent;text:modelData.n;color:index<2?Theme.blue:Theme.inkFaint;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro;font.weight:Font.Bold}}
                                Text{anchors.verticalCenter:parent.verticalCenter;text:modelData.t;color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall}
                            }}
                        }
                        PremiumButton{anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:48;compact:true;text:"基于此模板创建任务";iconName:"play";variant:"primary";accent:Theme.violet;onClicked:{app.navigate("runner");app.showToast("任务胶囊已创建，设备与变量检查通过")}}
                    }
                }
                GlassPanel {
                    Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:root.compactLayout?108:150;radius:22;elevated:false;panelColor:"#EEF8FFFF";panelColorBottom:"#E8F4FBFF"
                    Item{anchors.fill:parent;anchors.margins:root.compactLayout?14:18
                        Row{spacing:8;VectorIcon{width:18;height:18;name:"phone";color:Theme.blue}Text{text:"手机上传自定义模板";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.Bold}}
                        Text{y:27;width:parent.width;text:"小程序上传 JSON；仪表完成结构、引用和权限检查后才安装。";wrapMode:Text.WordWrap;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;lineHeight:1.3}
                        Row{anchors.left:parent.left;anchors.bottom:parent.bottom;spacing:7;StatusChip{height:24;text:"结构检查";accent:Theme.green;dot:false}StatusChip{height:24;text:"事务回滚";accent:Theme.green;dot:false}StatusChip{height:24;text:"权限沙箱";accent:Theme.green;dot:false}}
                    }
                }
            }
        }
    }
}
