import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import PressureOS 1.0
import "../components"

Item {
    id: root
    property int activePage: tasks.currentStage
    property var pendingPoint: null
    property string exportedPath: ""
    property bool syncingPage: false
    readonly property bool compactLayout: width < 1160 || height < 600
    readonly property bool modalActive: createDialog.opened || deletePointConfirm.visible
    readonly property bool automaticQuickCapture: tasks.isQuickTask && tasks.captureMode === "auto-index"
    readonly property bool engineeringCapture: tasks.captureTrustLevel === "engineering"
    readonly property bool zeroDriftTask: tasks.currentTemplateId === "quality.zero-drift"

    function statusColor(status) {
        if(status==="已完成") return Theme.green
        if(status==="待导出结果") return Theme.violet
        if(status==="待数据分析") return Theme.orange
        return Theme.blue
    }
    function simulatedTarget(x) {
        if(tasks.currentTemplateId==="metrology.multi-point") return Number(x)
        if(tasks.currentTemplateId==="quality.zero-drift") return 0
        if(tasks.currentTemplateId==="engineering.leak") return Math.max(0,300-Number(x)*1.2)
        return 12.4+0.82*Number(x)
    }
    function axisLabel() {
        return tasks.xVariableUnit === "" ? tasks.xVariableName
                                           : tasks.xVariableName + " / " + tasks.xVariableUnit
    }
    function measureGuidance() {
        if (!tasks.canCaptureCurrent && tasks.captureBlockReason !== "")
            return tasks.captureBlockReason
        if (root.engineeringCapture)
            return tasks.currentTemplateId === "quality.zero-drift"
                    ? "零压与读数稳定条件已满足；本点将保存为未冻结标定的工程记录"
                    : "协议和工况已满足；本点将保存为标定验收前的工程记录"
        if (root.automaticQuickCapture) {
            if (tasks.completedPoints < 3)
                return "保持工况稳定，点击右侧按钮采集第 " + tasks.nextAutoX.toFixed(0) + " 点"
            if (tasks.completedPoints < tasks.targetPoints)
                return "已经可以进入基础分析；也可继续采集到建议的 " + tasks.targetPoints + " 点"
            return "已达到建议点数；检查数据后进入基础分析"
        }
        if (tasks.isQuickTask && tasks.completedPoints >= 3)
            return "已经可以进入基础分析；继续填写“" + tasks.xVariableName + "”可增加数据"
        return tasks.completedPoints < tasks.targetPoints
                ? (device.hardwareMode ? "填写本点“"+tasks.xVariableName+"”，人工施加工况，稳定后采集"
                                       : "填写本点“"+tasks.xVariableName+"”，应用工况，等压力稳定后采集")
                : "目标点数已完成；检查数据后进入处理环节"
    }
    function gotoPage(index) { activePage=index; swipe.currentIndex=index }

    Connections {
        target: tasks
        function onCurrentTaskChanged() {
            root.syncingPage=true
            root.activePage=tasks.currentStage
            swipe.currentIndex=root.activePage
            root.exportedPath=""
            xField.text=tasks.currentTemplateId==="edu.pressure-mass.linear"?"500":"0"
            root.syncingPage=false
        }
    }

    Item {
        id: pageContent
        anchors.fill: parent
        enabled: !root.modalActive

    ColumnLayout {
        anchors.fill:parent
        anchors.leftMargin:root.compactLayout?18:25;anchors.rightMargin:root.compactLayout?18:25;anchors.topMargin:2;anchors.bottomMargin:3
        spacing:root.compactLayout?6:9
        RowLayout {
            Layout.fillWidth:true;Layout.preferredHeight:50
            GlassPanel{Layout.preferredWidth:48;Layout.preferredHeight:48;Layout.minimumWidth:48;Layout.maximumWidth:48;Layout.minimumHeight:48;Layout.maximumHeight:48;radius:16;material:"clear";interactive:true;elevated:true;accessibleName:"返回任务列表";VectorIcon{anchors.centerIn:parent;width:16;height:16;name:"back";color:Theme.inkMuted}onClicked:app.navigate("tasks")}
            ColumnLayout{spacing:0;Text{text:tasks.currentTemplateName; color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{text:tasks.currentTaskTitle;color:Theme.inkStrong;font.family:Theme.fontFamily;font.pixelSize:root.compactLayout?19:20;font.weight:Font.Bold;elide:Text.ElideRight;Layout.maximumWidth:root.compactLayout?340:430}}
            StatusChip{text:tasks.currentStatus;accent:root.statusColor(tasks.currentStatus);iconName:tasks.currentStatus==="已完成"?"check":"pulse"}
            StatusChip{visible:!root.compactLayout;text:tasks.lastSavedText;accent:Theme.green;iconName:"database"}
            Item{Layout.fillWidth:true}
            PremiumButton{Layout.preferredWidth:root.compactLayout?120:124;Layout.preferredHeight:48;compact:true;text:"任务列表";iconName:"task";variant:"ghost";onClicked:app.navigate("tasks")}
            PremiumButton{Layout.preferredWidth:root.compactLayout?122:132;Layout.preferredHeight:48;compact:true;text:"新建任务";iconName:"play";variant:"primary";onClicked:createDialog.opened=true}
        }

        RowLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;spacing:11
            GlassPanel {
                Layout.preferredWidth:root.compactLayout?208:236;Layout.minimumWidth:Layout.preferredWidth;Layout.maximumWidth:Layout.preferredWidth;Layout.fillHeight:true;radius:22
                Item{anchors.fill:parent;anchors.margins:root.compactLayout?12:14
                    Row{anchors.left:parent.left;anchors.top:parent.top;spacing:7;Text{text:"任务";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.Bold}StatusChip{height:23;text:tasks.taskList.length+" 项";accent:Theme.blue;dot:false}}
                    GlassPanel{anchors.right:parent.right;anchors.top:parent.top;width:44;height:44;radius:14;material:"clear";interactive:true;elevated:false;accessibleName:"新建任务";VectorIcon{anchors.centerIn:parent;width:16;height:16;name:"play";color:Theme.blue}onClicked:createDialog.opened=true}
                    Text{y:32;text:"按最后编辑时间排列";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                    ListView {
                        id:sideTasks;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.topMargin:52;anchors.bottom:autosave.top;anchors.bottomMargin:10
                        clip:true;spacing:6;model:tasks.taskList
                        boundsBehavior:Flickable.DragOverBounds;boundsMovement:Flickable.FollowBoundsBehavior
                        flickDeceleration:2200;maximumFlickVelocity:4200;pressDelay:80
                        delegate:Rectangle{
                            width:sideTasks.width;height:68;radius:14
                            color:modelData.id===tasks.currentTaskId?"#E9F4FF":(sideHover.hovered?"#F2F7FB":"transparent")
                            border.width:modelData.id===tasks.currentTaskId?1:0;border.color:"#B9DAF5"
                            Rectangle{x:8;y:10;width:32;height:32;radius:10;color:modelData.accent;opacity:.96;VectorIcon{anchors.centerIn:parent;width:16;height:16;name:modelData.icon;color:"white"}}
                            Text{x:48;y:9;width:parent.width-56;text:modelData.name;color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold;elide:Text.ElideRight}
                            Text{x:48;y:28;width:parent.width-56;text:modelData.status;color:modelData.statusColor;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;font.weight:Font.DemiBold}
                            Text{x:8;y:51;width:parent.width-16;text:modelData.updated+" · "+modelData.completedPoints+" 点";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;elide:Text.ElideRight}
                            HoverHandler{id:sideHover}
                            TapHandler{onTapped:tasks.selectTask(modelData.id)}
                        }
                    }
                    Rectangle{id:autosave;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:62;radius:15;color:"#EAF8F3";Rectangle{x:11;y:12;width:28;height:28;radius:9;color:Theme.green;VectorIcon{anchors.centerIn:parent;width:14;height:14;name:"database";color:"white"}}Text{x:48;y:11;text:"已自动保存";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold}Text{x:48;y:29;width:parent.width-58;text:"采点、删点、切换自动保存";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;wrapMode:Text.WordWrap}}
                }
            }

            GlassPanel {
                Layout.fillWidth:true;Layout.fillHeight:true;radius:23;clip:true
                Item{anchors.fill:parent
                    Rectangle{
                        id:stageBar;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;height:root.compactLayout?60:68;color:"#CFF9FCFF"
                        Row{anchors.centerIn:parent;spacing:3
                            Repeater{model:[{i:"info",t:"了解任务"},{i:"record",t:"数据记录"},{i:"filter",t:"数据处理"},{i:"chart",t:"结果分析"},{i:"export",t:"导出归档"}];delegate:Item{width:root.compactLayout?Math.floor((stageBar.width-24)/5):132;height:root.compactLayout?48:52
                                Rectangle{anchors.fill:parent;radius:15;color:root.activePage===index?"white":"transparent";border.width:root.activePage===index?1:0;border.color:Theme.line;Rectangle{visible:root.activePage===index;anchors.fill:parent;anchors.topMargin:5;z:-1;radius:parent.radius;color:Theme.shadow;opacity:.08}}
                                Row{anchors.centerIn:parent;spacing:7;Rectangle{width:27;height:27;radius:9;color:root.activePage===index?Theme.blue:(index<root.activePage?Theme.greenSoft:"#E8F1F7");VectorIcon{anchors.centerIn:parent;width:14;height:14;name:index<root.activePage?"check":modelData.i;color:root.activePage===index?"white":(index<root.activePage?Theme.green:Theme.inkFaint)}}Text{text:modelData.t;color:root.activePage===index?Theme.ink:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;font.weight:root.activePage===index?Font.DemiBold:Font.Normal;anchors.verticalCenter:parent.verticalCenter}}
                                TapHandler{onTapped:root.gotoPage(index)}
                            }}
                        }
                        Text{visible:!root.compactLayout;anchors.right:parent.right;anchors.rightMargin:15;anchors.bottom:parent.bottom;anchors.bottomMargin:4;text:"可左右滑动";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                    }
                    SwipeView {
                        id:swipe;anchors.left:parent.left;anchors.right:parent.right;anchors.top:stageBar.bottom;anchors.bottom:parent.bottom
                        currentIndex:root.activePage;interactive:true;clip:true
                        onCurrentIndexChanged:{root.activePage=currentIndex;if(!root.syncingPage)tasks.setCurrentStage(currentIndex)}

                        Item {
                            id:introPage
                            clip:true
                            RowLayout{anchors.fill:parent;anchors.margins:root.compactLayout?12:19;spacing:root.compactLayout?9:13
                                ColumnLayout{Layout.fillWidth:true;Layout.fillHeight:true;spacing:root.compactLayout?8:11
                                    Rectangle{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?88:123;radius:20;gradient:Gradient{orientation:Gradient.Horizontal;GradientStop{position:0;color:"#E9F5FF"}GradientStop{position:1;color:"#F5F1FF"}}
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?14:18
                                            StatusChip{text:"开始前先花 1 分钟了解任务";accent:Theme.blue;iconName:"spark"}
                                            Text{y:root.compactLayout?32:35;width:parent.width-(root.compactLayout?56:65);text:tasks.taskObjective;color:Theme.inkStrong;font.family:Theme.fontFamily;font.pixelSize:root.compactLayout?13:15;font.weight:Font.DemiBold;wrapMode:Text.WordWrap;maximumLineCount:2;elide:Text.ElideRight;lineHeight:1.2}
                                            Rectangle{anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;width:root.compactLayout?44:56;height:width;radius:root.compactLayout?14:18;color:Theme.blue;VectorIcon{anchors.centerIn:parent;width:root.compactLayout?22:28;height:width;name:"flask";color:"white"}}
                                        }
                                    }
                                    GlassPanel{Layout.fillWidth:true;Layout.fillHeight:true;radius:20;elevated:false
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:17
                                            Text{text:"这项任务怎么做";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.Bold}
                                            Text{anchors.right:parent.right;text:"模板流程 · 共 "+tasks.workflow.length+" 步";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                                            Column{y:root.compactLayout?28:32;width:parent.width;spacing:root.compactLayout?0:3
                                                Repeater{model:tasks.workflow;delegate:Row{width:parent.width;height:root.compactLayout?38:56;spacing:root.compactLayout?9:11
                                                    Item{width:32;height:root.compactLayout?36:49;Rectangle{anchors.horizontalCenter:parent.horizontalCenter;width:root.compactLayout?25:27;height:width;radius:8;color:index===0?Theme.blue:"#EAF2F8";Text{anchors.centerIn:parent;text:"0"+modelData.index;color:index===0?"white":Theme.inkFaint;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro;font.weight:Font.Bold}}Rectangle{visible:index<tasks.workflow.length-1;anchors.horizontalCenter:parent.horizontalCenter;y:root.compactLayout?27:30;width:1;height:root.compactLayout?11:24;color:Theme.line}}
                                                    Column{anchors.verticalCenter:parent.verticalCenter;width:parent.width-43;spacing:2;Text{text:modelData.title;color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textBody;font.weight:Font.DemiBold}Text{width:parent.width;text:modelData.detail;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;elide:Text.ElideRight}}
                                                }}
                                            }
                                        }
                                    }
                                }
                                ColumnLayout{Layout.preferredWidth:root.compactLayout?228:285;Layout.minimumWidth:Layout.preferredWidth;Layout.maximumWidth:Layout.preferredWidth;Layout.fillHeight:true;spacing:root.compactLayout?8:11
                                    GlassPanel{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?126:190;radius:20
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:16
                                            Row{spacing:7;VectorIcon{width:16;height:16;name:"check";color:Theme.green}Text{text:"准备清单";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.Bold}}
                                            Column{y:root.compactLayout?27:31;width:parent.width;spacing:root.compactLayout?5:9;Repeater{model:tasks.preparationItems;delegate:Row{width:parent.width;spacing:8;Rectangle{width:19;height:19;radius:6;color:Theme.greenSoft;VectorIcon{anchors.centerIn:parent;width:10;height:10;name:"check";color:Theme.green}}Text{width:parent.width-28;text:modelData;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;elide:Text.ElideRight}}}
                                            }
                                        }
                                    }
                                    GlassPanel{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?102:145;radius:20;elevated:false;panelColor:"#FFF8EEFF";panelColorBottom:"#FFF2E5FF"
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:16
                                            Row{spacing:7;VectorIcon{width:16;height:16;name:"warning";color:Theme.orange}Text{text:"安全提示";color:"#80501C";font.family:Theme.fontFamily;font.pixelSize:11;font.weight:Font.Bold}}
                                            Column{y:root.compactLayout?27:31;width:parent.width;spacing:root.compactLayout?5:9;Repeater{model:tasks.safetyNotes;delegate:Text{width:parent.width;text:"•  "+modelData;color:"#805E36";font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;wrapMode:Text.WordWrap;maximumLineCount:root.compactLayout?1:2;elide:Text.ElideRight}}}
                                        }
                                    }
                                    Item{Layout.fillHeight:true}
                                    PremiumButton{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?44:48;text:"我已了解，开始测量";iconName:"arrow";variant:"primary";onClicked:{tasks.setCurrentStage(1);root.gotoPage(1)}}
                                }
                            }
                        }

                        Item {
                            id:measurePage
                            clip:true
                            Item{anchors.fill:parent;anchors.margins:root.compactLayout?12:17
                                Rectangle{id:guidance;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;height:root.compactLayout?44:48;radius:15;color:"#EAF5FF";Rectangle{x:11;anchors.verticalCenter:parent.verticalCenter;width:29;height:29;radius:9;color:Theme.blue;Text{anchors.centerIn:parent;text:root.automaticQuickCapture?tasks.nextAutoX.toFixed(0):Math.min(tasks.completedPoints+1,tasks.targetPoints);color:"white";font.family:Theme.numberFont;font.pixelSize:11;font.weight:Font.Bold}}Text{x:51;width:parent.width-185;anchors.verticalCenter:parent.verticalCenter;text:root.measureGuidance();color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textBody;font.weight:Font.DemiBold;elide:Text.ElideRight}Text{anchors.right:parent.right;anchors.rightMargin:13;anchors.verticalCenter:parent.verticalCenter;text:tasks.isQuickTask?(tasks.completedPoints+" 点 · 建议 "+tasks.targetPoints):(tasks.completedPoints+" / "+tasks.targetPoints+" 点");color:Theme.blue;font.family:Theme.numberFont;font.pixelSize:11;font.weight:Font.Bold}}
                                Row{id:captureRow;anchors.left:parent.left;anchors.right:parent.right;anchors.top:guidance.bottom;anchors.topMargin:root.compactLayout?8:10;height:60;spacing:root.compactLayout?7:8;clip:true
                                    Rectangle{width:root.compactLayout?164:190;height:58;radius:15;color:"#F8FBFE";border.width:1;border.color:xField.activeFocus?Theme.blue:Theme.line
                                        Text{x:12;y:6;text:root.automaticQuickCapture?"自动编号 · 无需输入":tasks.xVariableName+" · 手动输入";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                                        TextField{id:xField;visible:!root.automaticQuickCapture;x:9;y:19;width:125;height:36;text:tasks.currentTemplateId==="edu.pressure-mass.linear"?"500":"0";selectByMouse:true;color:Theme.ink;font.family:Theme.numberFont;font.pixelSize:17;font.weight:Font.DemiBold;validator:DoubleValidator{bottom:-100000;top:100000;decimals:4} background:Item{} onPressed:app.openKeyboard(xField,"numeric")}
                                        Text{visible:root.automaticQuickCapture;x:12;y:23;text:"第 "+tasks.nextAutoX.toFixed(0)+" 点";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:17;font.weight:Font.DemiBold}
                                        Text{visible:!root.automaticQuickCapture;anchors.right:parent.right;anchors.rightMargin:12;y:31;text:tasks.xVariableUnit;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textBody}
                                    }
                                    PremiumButton{width:root.compactLayout?136:126;height:58;text:device.hardwareMode?"工况提示":"应用工况";iconName:"target";variant:"secondary";onClicked:{if(device.hardwareMode)app.showToast("请人工施加工况，待实时压力稳定后采集");else{device.setTarget(root.simulatedTarget(Number(xField.text)));app.showToast("Demo 压力源正在向目标工况收敛")}}}
                                    Rectangle{width:root.compactLayout?162:184;height:58;radius:15;color:"#F8FBFE";border.width:1;border.color:device.stable?"#A9E5D4":Theme.line
                                        Text{x:12;y:6;text:device.hardwareMode?"下位机实时压力":"内置压力通道";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                                        Text{x:12;y:23;text:device.pressureKPa.toFixed(3);color:Theme.ink;font.family:Theme.numberFont;font.pixelSize:17;font.weight:Font.DemiBold}
                                        Text{anchors.right:parent.right;anchors.rightMargin:12;y:29;text:"kPa";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall}
                                    }
                                    StatusChip{width:root.compactLayout?88:implicitWidth;height:34;anchors.verticalCenter:parent.verticalCenter;text:root.engineeringCapture?(device.stable?"工程记录":"工程·收敛"):(tasks.captureTrustLevel==="blocked"?"暂不可用":(device.stable?"已稳定":"收敛中"));accent:root.engineeringCapture?Theme.orange:(tasks.captureTrustLevel==="blocked"?Theme.red:(device.stable?Theme.green:Theme.orange));iconName:root.engineeringCapture?"info":(tasks.captureTrustLevel==="blocked"?"warning":(device.stable?"check":"pulse"))}
                                    PremiumButton{width:root.compactLayout?150:148;height:58;text:tasks.captureTrustLevel==="blocked"?"暂不可采集":(root.engineeringCapture?"采集工程点":(root.automaticQuickCapture?"采集本点":"采集并保存"));iconName:tasks.captureTrustLevel==="blocked"?"info":"record";variant:"primary";enabled:tasks.canCaptureCurrent;onClicked:{const x=root.automaticQuickCapture?tasks.nextAutoX:Number(xField.text);if(tasks.capturePoint(x,device.pressureKPa,device.stable)){if(!root.automaticQuickCapture)xField.text=(Number(xField.text)+(tasks.currentTemplateId==="edu.pressure-mass.linear"?100:1)).toString();device.setTarget(root.simulatedTarget(root.automaticQuickCapture?tasks.nextAutoX:Number(xField.text)))}}}
                                }
                                Item {
                                    id: dataToolbar
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: captureRow.bottom
                                    anchors.topMargin: 4
                                    height: 48

                                    Row {
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 8
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: "任务数据"
                                            color: Theme.ink
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.textSmall
                                            font.weight: Font.Bold
                                        }
                                        StatusChip {
                                            height: 24
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: "每次操作自动保存"
                                            accent: Theme.green
                                            iconName: "database"
                                        }
                                    }
                                    PremiumButton {
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 148
                                        height: 48
                                        compact: true
                                        text: tasks.isQuickTask ? "进入基础分析" : "完成测量阶段"
                                        iconName: "arrow"
                                        variant: tasks.canFinishMeasurement ? "primary" : "secondary"
                                        enabled: tasks.canFinishMeasurement
                                        onClicked: if (tasks.finishMeasurement()) root.gotoPage(2)
                                    }
                                }
                                Rectangle{id:dataHeader;anchors.left:parent.left;anchors.right:parent.right;anchors.top:dataToolbar.bottom;anchors.topMargin:4;height:30;radius:9;color:"#EDF4F9"
                                    Row{anchors.fill:parent;anchors.leftMargin:10;Text{width:40;anchors.verticalCenter:parent.verticalCenter;text:"#";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{width:130;anchors.verticalCenter:parent.verticalCenter;text:root.axisLabel();color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{width:145;anchors.verticalCenter:parent.verticalCenter;text:"稳定压力 / kPa";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{width:210;anchors.verticalCenter:parent.verticalCenter;text:"数据来源";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{width:80;anchors.verticalCenter:parent.verticalCenter;text:"状态";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;horizontalAlignment:Text.AlignHCenter}Text{width:56;anchors.verticalCenter:parent.verticalCenter;text:"操作";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;horizontalAlignment:Text.AlignHCenter}}
                                }
                                ListView{id:pointList;anchors.left:parent.left;anchors.right:parent.right;anchors.top:dataHeader.bottom;anchors.bottom:parent.bottom;clip:true;model:tasks.rows;boundsBehavior:Flickable.DragOverBounds;boundsMovement:Flickable.FollowBoundsBehavior;flickDeceleration:2200;maximumFlickVelocity:4200;pressDelay:80
                                    delegate:Rectangle{width:pointList.width;height:48;color:index%2===0?"#35FFFFFF":"transparent";Row{anchors.fill:parent;anchors.leftMargin:10
                                        Text{width:40;anchors.verticalCenter:parent.verticalCenter;text:modelData.index;color:Theme.inkFaint;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro}
                                        Text{width:130;anchors.verticalCenter:parent.verticalCenter;text:root.automaticQuickCapture?Number(modelData.mass).toFixed(0):Number(modelData.mass).toFixed(3);color:Theme.ink;font.family:Theme.numberFont;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold}
                                        Text{width:145;anchors.verticalCenter:parent.verticalCenter;text:Number(modelData.pressure).toFixed(3);color:Theme.ink;font.family:Theme.numberFont;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold}
                                        Text{width:210;anchors.verticalCenter:parent.verticalCenter;text:"内置压力通道 · 稳定均值";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;elide:Text.ElideRight}
                                        Item{width:80;height:parent.height;StatusChip{width:72;height:24;anchors.centerIn:parent;text:modelData.status;accent:modelData.suspect?Theme.orange:Theme.green;dot:false}}
                                        Item{
                                            id: pointDeleteButton
                                            width: 56
                                            height: parent.height
                                            activeFocusOnTab: true
                                            Accessible.role: Accessible.Button
                                            Accessible.name: "删除测量点"
                                            function requestDelete() {
                                                root.pendingPoint = modelData
                                                deletePointConfirm.visible = true
                                            }
                                            Rectangle {
                                                anchors.centerIn: parent
                                                width: 34
                                                height: 34
                                                radius: 11
                                                color: pointDeleteTap.pressed ? "#24E95A6F" : (pointDeleteHover.hovered ? "#18E95A6F" : "#42FFFFFF")
                                                border.width: 1
                                                border.color: pointDeleteButton.activeFocus ? Theme.red : "#80FFFFFF"
                                                DeleteIcon {
                                                    anchors.centerIn: parent
                                                    width: 14
                                                    height: 14
                                                    color: pointDeleteHover.hovered || pointDeleteTap.pressed ? Theme.red : Theme.inkFaint
                                                }
                                            }
                                            HoverHandler { id: pointDeleteHover }
                                            TapHandler {
                                                id: pointDeleteTap
                                                gesturePolicy: TapHandler.ReleaseWithinBounds
                                                onTapped: pointDeleteButton.requestDelete()
                                            }
                                            Keys.onEnterPressed: requestDelete()
                                            Keys.onSpacePressed: requestDelete()
                                        }
                                    }Rectangle{anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:1;color:Theme.lineSoft}}
                                }
                            }
                        }

                        Item {
                            id:processPage
                            clip:true
                            RowLayout{anchors.fill:parent;anchors.margins:root.compactLayout?12:19;spacing:root.compactLayout?9:13
                                ColumnLayout{Layout.fillWidth:true;Layout.fillHeight:true;spacing:root.compactLayout?8:11
                                    Rectangle{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?64:78;radius:18;color:"#EAF5FF";Item{anchors.fill:parent;anchors.margins:root.compactLayout?12:14;Rectangle{width:root.compactLayout?36:40;height:width;radius:12;color:Theme.blue;VectorIcon{anchors.centerIn:parent;width:18;height:18;name:root.zeroDriftTask?"pulse":"filter";color:"white"}}Text{x:root.compactLayout?48:53;y:0;text:root.zeroDriftTask?"按时间观察零点是否真正稳定":(tasks.isQuickTask?"快速任务采用基础处理流程":"模板已锁定数据处理口径");color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.Bold}Text{x:root.compactLayout?48:53;y:23;width:parent.width-x;text:root.zeroDriftTask?"时间排序 → 周期中心 → 漂移趋势 → 稳定性结论":(tasks.isQuickTask?"生成数据表 → 线性试拟合 → 查看残差与 R² → 由用户判断模型是否适用":"统一单位 → 有效性检查 → 普通最小二乘拟合 → 残差与不确定度计算");color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;elide:Text.ElideRight}}}
                                    GlassPanel{Layout.fillWidth:true;Layout.fillHeight:true;radius:20;elevated:false
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:17
                                            Text{text:"处理流水线";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.Bold}
                                            Row{id:pipeline;y:root.compactLayout?30:37;width:parent.width;height:root.compactLayout?62:72;spacing:root.compactLayout?3:7
                                                Repeater{model:root.zeroDriftTask?[{n:"01",t:"时序数据",d:tasks.completedPoints+" 个零点"},{n:"02",t:"完整性",d:"时间 · 来源"},{n:"03",t:"周期中心",d:"抑制规律起伏"},{n:"04",t:"漂移趋势",d:"kPa / min"},{n:"05",t:"稳定结论",d:"极差 · 标准差"}]:[{n:"01",t:"原始数据",d:tasks.completedPoints+" 个有效点"},{n:"02",t:"单位归一",d:(tasks.xVariableUnit===""?"序号":tasks.xVariableUnit)+" / kPa"},{n:"03",t:"有效性检查",d:"范围 · 缺失 · 稳定"},{n:"04",t:tasks.isQuickTask?"线性试拟合":"线性拟合",d:"OLS · y 对 x"},{n:"05",t:"统计输出",d:"残差 · u · R²"}];delegate:Item{width:(pipeline.width-pipeline.spacing*4)/5;height:pipeline.height;Rectangle{width:parent.width-(index<4?(root.compactLayout?10:17):0);height:parent.height;radius:root.compactLayout?13:15;color:index<3?"#F0F7FC":"#EEF2FF";Text{x:8;y:7;text:modelData.n;color:index<3?Theme.blue:Theme.violet;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro;font.weight:Font.Bold}Text{x:8;y:24;width:parent.width-14;text:modelData.t;color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold;elide:Text.ElideRight}Text{x:8;y:43;width:parent.width-14;text:modelData.d;color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;elide:Text.ElideRight}}VectorIcon{visible:index<4;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;width:9;height:9;name:"arrow";color:Theme.inkFaint}}
                                                }
                                            }
                                            Text{y:root.compactLayout?105:128;text:root.zeroDriftTask?"本任务会给出":"计算口径说明";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:11;font.weight:Font.DemiBold}
                                            Column{y:root.compactLayout?126:153;width:parent.width;spacing:root.compactLayout?5:9
                                                Repeater{model:root.zeroDriftTask?[
                                                    "零点中心：按完整振荡周期计算时间加权均值",
                                                    "漂移速度：显示零点随时间变化的方向和斜率",
                                                    "稳定程度：综合极差、标准差和异常扰动给出判断"
                                                ]:[
                                                    tasks.isQuickTask?"试拟合：p = ax + b；结果不是自动结论，需要结合 R² 与残差判断":"模型：p = ax + b；x 为“"+tasks.xVariableName+"”，p 为稳定压力",
                                                    "拟合：最小化压力方向残差平方和 Σ[pᵢ − (axᵢ+b)]²",
                                                    "粗差预筛：|标准化残差| > 2.5 仅标记嫌疑，不自动删点",
                                                    "不确定度：当前 Demo 合成回归残差 A 类与 0.1 kPa 分辨力 B 类"
                                                ];delegate:Row{width:parent.width;height:root.compactLayout?22:implicitHeight;spacing:8;Rectangle{width:20;height:20;radius:7;color:Theme.blueSoft;VectorIcon{anchors.centerIn:parent;width:10;height:10;name:"check";color:Theme.blue}}Text{width:Math.max(0,parent.width-28);text:modelData;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;elide:root.compactLayout?Text.ElideRight:Text.ElideNone;wrapMode:root.compactLayout?Text.NoWrap:Text.WordWrap}}
                                            }
                                        }
                                    }
                                }
                                }
                                ColumnLayout{Layout.preferredWidth:root.compactLayout?220:250;Layout.minimumWidth:Layout.preferredWidth;Layout.maximumWidth:Layout.preferredWidth;Layout.fillHeight:true;spacing:root.compactLayout?8:11
                                    GlassPanel{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?150:192;radius:20
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:16
                                            Text{text:"数据门禁";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.Bold}
                                            Column{y:29;width:parent.width;spacing:root.compactLayout?7:10
                                                Repeater{model:root.zeroDriftTask?[{t:"零点记录 ≥ 3",ok:tasks.completedPoints>=3},{t:"记录覆盖观察时段",ok:tasks.completedPoints>=tasks.targetPoints},{t:"压力保持在零点附近",ok:true},{t:"时间与数据来源完整",ok:true}]:[{t:"有效点数 ≥ 3",ok:tasks.completedPoints>=3},{t:tasks.isQuickTask?"达到建议点数（非强制）":"已达到模板目标点数",ok:tasks.completedPoints>=tasks.targetPoints},{t:"压力均在工作量程内",ok:true},{t:"数据来源与单位完整",ok:true}];delegate:Row{spacing:8;Rectangle{width:19;height:19;radius:6;color:modelData.ok?Theme.greenSoft:Theme.orangeSoft;VectorIcon{anchors.centerIn:parent;width:10;height:10;name:modelData.ok?"check":"warning";color:modelData.ok?Theme.green:Theme.orange}}Text{text:modelData.t;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}}
                                            }
                                        }
                                    }
                                    }
                                    GlassPanel{Layout.fillWidth:true;Layout.fillHeight:true;radius:20;elevated:false;panelColor:"#F1F7FCFF";panelColorBottom:"#EAF4FAFF"
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:16
                                            VectorIcon{width:21;height:21;name:"spark";color:Theme.violet}
                                            Text{y:31;text:root.zeroDriftTask?"规律起伏会怎样处理？":"为什么不自动删异常点？";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textBody;font.weight:Font.DemiBold}
                                            Text{y:53;width:parent.width;text:root.zeroDriftTask?"规律振荡会按完整周期求中心，不会被当成一次错误读数；突发扰动仍会单独标记。":"异常可能来自误操作，也可能是真实物理现象。系统只提示证据，由用户核查后决定是否删除。";wrapMode:Text.WordWrap;maximumLineCount:root.compactLayout?3:5;elide:Text.ElideRight;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;lineHeight:1.3}
                                            PremiumButton{anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:48;compact:true;text:tasks.hasFit?"执行分析并查看结果":"返回补充数据";iconName:tasks.hasFit?"chart":"back";variant:tasks.hasFit?"primary":"secondary";onClicked:{if(tasks.hasFit){tasks.setCurrentStage(3);root.gotoPage(3)}else root.gotoPage(1)}}
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            id:analysisPage
                            clip:true
                            RowLayout{anchors.fill:parent;anchors.margins:root.compactLayout?12:17;spacing:root.compactLayout?9:12
                                ColumnLayout{Layout.fillWidth:true;Layout.fillHeight:true;spacing:root.compactLayout?7:10
                                    GlassPanel{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?180:235;radius:20;elevated:false
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:16
                                            Row{spacing:8;Text{text:root.zeroDriftTask?"零点趋势结果":(tasks.isQuickTask?"基础线性试拟合":"线性拟合结果");color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.Bold}StatusChip{height:23;text:tasks.fitQuality;accent:tasks.rSquared>=.99?Theme.green:Theme.orange;dot:true}}
                                            FitChart{anchors.left:parent.left;anchors.right:parent.right;y:root.compactLayout?28:31;height:root.compactLayout?100:145;rows:tasks.rows;slope:tasks.slope;intercept:tasks.intercept}
                                            Rectangle{anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:root.compactLayout?31:35;radius:11;color:"#EAF4FF";Text{anchors.left:parent.left;anchors.leftMargin:12;anchors.verticalCenter:parent.verticalCenter;text:tasks.equation;color:Theme.blueDeep;font.family:Theme.numberFont;font.pixelSize:12;font.weight:Font.DemiBold}Text{anchors.right:parent.right;anchors.rightMargin:12;anchors.verticalCenter:parent.verticalCenter;text:"R² = "+(tasks.hasFit?tasks.rSquared.toFixed(6):"—");color:Theme.inkMuted;font.family:Theme.numberFont;font.pixelSize: Theme.textBody}}
                                        }
                                    }
                                    RowLayout{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?62:70;spacing:7
                                        MetricCard{Layout.fillWidth:true;Layout.fillHeight:true;label:"相关系数 R";value:tasks.hasFit?tasks.pearsonR.toFixed(6):"—";iconName:"trend";accent:Theme.blue}
                                        MetricCard{Layout.fillWidth:true;Layout.fillHeight:true;label:"残差标准差";value:tasks.hasFit?tasks.residualStd.toFixed(3):"—";unit:"kPa";iconName:"pulse";accent:Theme.orange}
                                        MetricCard{Layout.fillWidth:true;Layout.fillHeight:true;label:"最大绝对残差";value:tasks.hasFit?tasks.maxAbsResidual.toFixed(3):"—";unit:"kPa";iconName:"target";accent:Theme.violet}
                                    }
                                    GlassPanel{Layout.fillWidth:true;Layout.fillHeight:true;radius:20
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?12:15
                                            Text{text:"逐点残差审查";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:11;font.weight:Font.Bold}
                                            Text{anchors.right:parent.right;text:tasks.outlierSummary;color:tasks.outlierCount>0?Theme.orange:Theme.green;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                                            ListView{id:residualList;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.topMargin:24;anchors.bottom:parent.bottom;clip:true;model:tasks.rows;boundsBehavior:Flickable.DragOverBounds;boundsMovement:Flickable.FollowBoundsBehavior;flickDeceleration:2200;maximumFlickVelocity:4200;pressDelay:80;delegate:RowLayout{width:residualList.width;height:root.compactLayout?27:30;spacing:8
                                                Text{Layout.preferredWidth:28;text:"#"+modelData.index;color:Theme.inkFaint;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro}
                                                Text{Layout.preferredWidth:92;text:Number(modelData.residual).toFixed(4)+" kPa";color:modelData.suspect?Theme.red:Theme.ink;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro;font.weight:Font.DemiBold}
                                                Item{Layout.fillWidth:true;Layout.preferredHeight:12;Rectangle{anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter;width:Math.max(4,parent.width*(tasks.maxAbsResidual>0?Math.abs(Number(modelData.residual))/tasks.maxAbsResidual:0));height:6;radius:3;color:modelData.suspect?Theme.red:Theme.blue;opacity:.75}}
                                                Text{Layout.preferredWidth:66;text:"z = "+Number(modelData.standardizedResidual).toFixed(2);color:Theme.inkFaint;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro}
                                                StatusChip{Layout.preferredWidth:62;height:20;text:modelData.suspect?"待核对":"正常";accent:modelData.suspect?Theme.orange:Theme.green;dot:false}
                                            }}
                                        }
                                    }
                                }
                                ColumnLayout{Layout.preferredWidth:root.compactLayout?230:270;Layout.minimumWidth:Layout.preferredWidth;Layout.maximumWidth:Layout.preferredWidth;Layout.fillHeight:true;spacing:root.compactLayout?7:10
                                    GlassPanel{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?180:235;radius:20
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?12:16
                                            Text{text:"不确定度概览";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.Bold}
                                            Text{anchors.right:parent.right;text:"Demo · k=2";color:Theme.violet;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro;font.weight:Font.DemiBold}
                                            Column{y:root.compactLayout?29:34;width:parent.width;spacing:root.compactLayout?4:7
                                                Repeater{model:[{l:"A 类标准不确定度",v:tasks.typeAUncertainty,c:Theme.blue},{l:"B 类（分辨力）",v:tasks.typeBUncertainty,c:Theme.violet},{l:"合成标准不确定度",v:tasks.combinedUncertainty,c:Theme.orange},{l:"扩展不确定度 U",v:tasks.expandedUncertainty,c:Theme.green}];delegate:Rectangle{width:parent.width;height:root.compactLayout?27:37;radius:10;color:"#F2F7FB";Rectangle{x:8;anchors.verticalCenter:parent.verticalCenter;width:6;height:root.compactLayout?18:22;radius:4;color:modelData.c}Text{x:22;anchors.verticalCenter:parent.verticalCenter;text:modelData.l;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{anchors.right:parent.right;anchors.rightMargin:8;anchors.verticalCenter:parent.verticalCenter;text:Number(modelData.v).toFixed(4)+" kPa";color:Theme.ink;font.family:Theme.numberFont;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold}}
                                            }
                                            Text{visible:!root.compactLayout;y:190;width:parent.width;text:"正式模板需补充标准器、温漂等全部 B 类分量及自由度。";wrapMode:Text.WordWrap;color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
                                        }
                                    }
                                    }
                                    GlassPanel{Layout.fillWidth:true;Layout.fillHeight:true;radius:20;elevated:false;panelColor:(tasks.outlierCount>0||(tasks.isQuickTask&&tasks.rSquared<.95))?"#FFF8EEFF":"#EEF9F5FF";panelColorBottom:(tasks.outlierCount>0||(tasks.isQuickTask&&tasks.rSquared<.95))?"#FFF2E5FF":"#E7F6F0FF"
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?12:16
                                            Row{spacing:8;VectorIcon{width:18;height:18;name:(tasks.outlierCount>0||(tasks.isQuickTask&&tasks.rSquared<.95))?"warning":"check";color:(tasks.outlierCount>0||(tasks.isQuickTask&&tasks.rSquared<.95))?Theme.orange:Theme.green}Text{text:tasks.outlierCount>0?"需要人工复核":((tasks.isQuickTask&&tasks.rSquared<.95)?"线性模型可能不适用":"分析检查通过");color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:11;font.weight:Font.DemiBold}}
                                            Text{y:29;width:parent.width;height:Math.max(0,actionRow.y-35);text:(tasks.isQuickTask&&tasks.rSquared<.95?"R² 较低，请把本页视为数据预览，不要直接下线性结论。 ":"")+tasks.outlierSummary+"。如确认误测，可回到数据记录页删除，统计量将即时重算。";wrapMode:Text.WordWrap;maximumLineCount:3;elide:Text.ElideRight;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;lineHeight:1.3}
                                            Row{id:actionRow;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:48;spacing:7;PremiumButton{visible:tasks.outlierCount>0;width:96;height:48;compact:true;text:"核对数据";iconName:"back";variant:"secondary";onClicked:root.gotoPage(1)}PremiumButton{width:tasks.outlierCount>0?parent.width-103:parent.width;height:48;compact:true;text:"确认并导出";iconName:"arrow";variant:"primary";onClicked:{if(tasks.confirmAnalysis())root.gotoPage(4)}}}
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            id:exportPage
                            clip:true
                            RowLayout{anchors.fill:parent;anchors.margins:root.compactLayout?12:20;spacing:root.compactLayout?9:15
                                ColumnLayout{Layout.fillWidth:true;Layout.fillHeight:true;spacing:root.compactLayout?7:12
                                    Rectangle{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?78:105;radius:21;gradient:Gradient{orientation:Gradient.Horizontal;GradientStop{position:0;color:"#E9F5FF"}GradientStop{position:1;color:"#EEF0FF"}}
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:18;Rectangle{width:root.compactLayout?42:50;height:width;radius:14;color:tasks.currentStatus==="已完成"?Theme.green:Theme.blue;VectorIcon{anchors.centerIn:parent;width:root.compactLayout?21:25;height:width;name:tasks.currentStatus==="已完成"?"check":"export";color:"white"}}Text{x:root.compactLayout?55:65;y:0;text:tasks.currentStatus==="已完成"?"这项任务已经完成":"最后一步：生成可追溯产物";color:Theme.inkStrong;font.family:Theme.fontFamily;font.pixelSize:root.compactLayout?15:17;font.weight:Font.Bold}Text{x:root.compactLayout?55:65;y:26;width:parent.width-x;text:tasks.currentStatus==="已完成"?"数据、方法和结果已锁定在同一任务胶囊中。":"依据模板生成数据表、分析摘要和报告矢量图。";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;wrapMode:Text.WordWrap;maximumLineCount:2;elide:Text.ElideRight}}
                                    }
                                    RowLayout{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?96:145;spacing:root.compactLayout?7:10
                                        Repeater{model:[{i:"file",t:"原始数据表",e:"CSV",d:"测量值、残差、来源与时间"},{i:"chart",t:"拟合结果图",e:"SVG",d:"可直接用于 PPT 和报告"},{i:"database",t:"分析证据包",e:"JSON",d:"模型、统计量与异常规则"}];delegate:GlassPanel{Layout.fillWidth:true;Layout.fillHeight:true;radius:19;elevated:false;Item{anchors.fill:parent;anchors.margins:root.compactLayout?11:15;Rectangle{width:root.compactLayout?30:36;height:width;radius:10;color:index===0?Theme.blueSoft:(index===1?"#EEF0FF":Theme.greenSoft);VectorIcon{anchors.centerIn:parent;width:root.compactLayout?15:18;height:width;name:modelData.i;color:index===0?Theme.blue:(index===1?Theme.violet:Theme.green)}}StatusChip{anchors.right:parent.right;height:23;text:modelData.e;accent:index===0?Theme.blue:(index===1?Theme.violet:Theme.green);dot:false}Text{y:root.compactLayout?37:48;text:modelData.t;color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:11;font.weight:Font.DemiBold}Text{y:root.compactLayout?56:70;width:parent.width;text:modelData.d;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;wrapMode:Text.WordWrap;maximumLineCount:2;elide:Text.ElideRight;lineHeight:1.2}}}}
                                    }
                                    GlassPanel{Layout.fillWidth:true;Layout.fillHeight:true;radius:20
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:17
                                            Text{text:"任务摘要";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:12;font.weight:Font.Bold}
                                            Grid{y:root.compactLayout?29:36;columns:root.compactLayout?3:2;columnSpacing:root.compactLayout?10:55;rowSpacing:root.compactLayout?7:13
                                                Repeater{model:[{l:"任务名称",v:tasks.currentTaskTitle},{l:"使用模板",v:tasks.currentTemplateName},{l:"有效数据",v:tasks.completedPoints+" 点"},{l:"拟合关系式",v:tasks.equation},{l:"决定系数",v:tasks.hasFit?tasks.rSquared.toFixed(6):"—"},{l:"扩展不确定度",v:tasks.hasFit?tasks.expandedUncertainty.toFixed(4)+" kPa":"—"}];delegate:Column{width:root.compactLayout?145:245;spacing:2;Text{text:modelData.l;color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{width:parent.width;text:modelData.v;color:Theme.ink;font.family:index>=3?Theme.numberFont:Theme.fontFamily;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold;elide:Text.ElideRight}}}
                                            }
                                        }
                                    }
                                }
                                ColumnLayout{Layout.preferredWidth:root.compactLayout?230:275;Layout.minimumWidth:Layout.preferredWidth;Layout.maximumWidth:Layout.preferredWidth;Layout.fillHeight:true;spacing:root.compactLayout?7:12
                                    GlassPanel{Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?184:220;radius:21;elevated:false;panelColor:"#EEF8FFFF";panelColorBottom:"#E9F2FFFF"
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:17
                                            VectorIcon{anchors.horizontalCenter:parent.horizontalCenter;width:root.compactLayout?34:40;height:width;name:"export";color:Theme.blue}
                                            Text{anchors.horizontalCenter:parent.horizontalCenter;y:root.compactLayout?42:52;text:"一键生成任务数据包";color:Theme.inkStrong;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.Bold}
                                            Text{y:root.compactLayout?66:79;width:parent.width;text:"文件写入系统“下载”目录，并按任务名称和时间建立独立文件夹。";horizontalAlignment:Text.AlignHCenter;wrapMode:Text.WordWrap;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;lineHeight:1.3}
                                            PremiumButton{anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:48;text:tasks.currentStatus==="已完成"?"再次导出":"生成并完成任务";iconName:"export";variant:"primary";onClicked:{root.exportedPath=tasks.exportBundle()}}
                                        }
                                    }
                                    GlassPanel{Layout.fillWidth:true;Layout.fillHeight:true;radius:21
                                        Item{anchors.fill:parent;anchors.margins:root.compactLayout?13:17
                                            Row{spacing:8;Rectangle{width:28;height:28;radius:9;color:root.exportedPath!==""?Theme.greenSoft:"#EDF4F9";VectorIcon{anchors.centerIn:parent;width:14;height:14;name:root.exportedPath!==""?"check":"clock";color:root.exportedPath!==""?Theme.green:Theme.inkFaint}}Column{anchors.verticalCenter:parent.verticalCenter;spacing:1;Text{text:root.exportedPath!==""?"最近导出成功":"等待生成";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold}Text{text:root.exportedPath!==""?"3 项产物已写入":"完成后在此显示位置";color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}}}
                                            Text{visible:root.exportedPath!=="";y:47;width:parent.width;text:root.exportedPath;wrapMode:Text.WrapAnywhere;color:Theme.blueDeep;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro;lineHeight:1.3}
                                            Rectangle{anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:root.compactLayout?52:62;radius:15;color:"#F2F7FB";VectorIcon{x:11;anchors.verticalCenter:parent.verticalCenter;width:17;height:17;name:"lock";color:Theme.violet}Text{x:39;anchors.verticalCenter:parent.verticalCenter;width:parent.width-49;text:"导出不会删除任务；之后可随时复核或再次生成。";wrapMode:Text.WordWrap;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}}
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    }

    CreateTaskDialog{id:createDialog;anchors.fill:parent;onTaskCreated:root.gotoPage(0)}
    Rectangle{id:deletePointConfirm;visible:false;anchors.fill:parent;color:Theme.modalScrim;z:150
        MouseArea{anchors.fill:parent;acceptedButtons:Qt.AllButtons}
        GlassPanel{anchors.centerIn:parent;width:420;height:218;radius:25;material:"modal";tintStrength:0.012;MouseArea{anchors.fill:parent;acceptedButtons:Qt.AllButtons;preventStealing:true}Item{anchors.fill:parent;anchors.margins:22
            Rectangle{width:40;height:40;radius:13;color:"#FFF0F2";VectorIcon{anchors.centerIn:parent;width:19;height:19;name:"delete";color:Theme.red}}
            Text{x:53;y:1;text:"删除这个测量点？";color:Theme.inkStrong;font.family:Theme.fontFamily;font.pixelSize:16;font.weight:Font.Bold}
            Text{x:53;y:26;text:root.pendingPoint?tasks.xVariableName+" "+Number(root.pendingPoint.mass).toFixed(3)+" "+tasks.xVariableUnit+" · "+Number(root.pendingPoint.pressure).toFixed(3)+" kPa":"";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}
            Text{y:64;width:parent.width;text:"删除后，拟合、残差和不确定度会立即重新计算。任务的其他数据不会受到影响。";wrapMode:Text.WordWrap;color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall;lineHeight:1.4}
            Row{anchors.right:parent.right;anchors.bottom:parent.bottom;spacing:9;PremiumButton{width:96;height:48;compact:true;text:"取消";variant:"ghost";onClicked:deletePointConfirm.visible=false}PremiumButton{width:132;height:48;compact:true;text:"删除并重算";iconName:"delete";variant:"danger";onClicked:{if(root.pendingPoint)tasks.deletePoint(root.pendingPoint.databaseId);deletePointConfirm.visible=false}}
        }}
    }
    }
}
