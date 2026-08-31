import QtQuick 2.15
import QtQuick.Layouts 1.15
import PressureOS 1.0
import "../components"

Item {
    id: root
    property bool showResiduals: true
    readonly property bool compactLayout: width < 1160 || height < 600

    ColumnLayout {
        anchors.fill:parent
        anchors.leftMargin:root.compactLayout?20:32;anchors.rightMargin:root.compactLayout?20:32;anchors.topMargin:3;anchors.bottomMargin:4
        spacing:root.compactLayout?8:12
        RowLayout {
            Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?48:52
            ColumnLayout{spacing:0;Text{text:"任务胶囊 / "+tasks.currentTaskTitle;color:Theme.inkFaint;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{text:"数据工作室";color:Theme.inkStrong;font.family:Theme.fontFamily;font.pixelSize:root.compactLayout?22:24;font.weight:Font.Bold}}
            StatusChip{text:tasks.completedPoints+" 个有效数据点";accent:Theme.green;iconName:"check"}
            Item{Layout.fillWidth:true}
            PremiumButton{width:112;height:48;compact:true;text:"残差线";iconName:"pulse";checked:root.showResiduals;onClicked:root.showResiduals=!root.showResiduals}
            PremiumButton{width:132;height:48;compact:true;text:"导出 CSV";iconName:"export";variant:"primary";onClicked:{const p=tasks.exportCsv();if(p)app.showToast("已导出："+p,4200)}}
        }

        RowLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;spacing:root.compactLayout?10:15
            ColumnLayout {
                Layout.fillWidth:true;Layout.fillHeight:true;spacing:root.compactLayout?8:12
                GlassPanel {
                    Layout.fillWidth:true;Layout.fillHeight:true;Layout.preferredHeight:root.compactLayout?286:390;radius:24
                    Item{anchors.fill:parent;anchors.margins:root.compactLayout?15:20
                        Row{spacing:9;Text{text:"压力—质量线性拟合";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:14;font.weight:Font.Bold}StatusChip{height:24;text:"自动计算 · v1.0";accent:Theme.blue;iconName:"spark"}}
                        Text{anchors.right:parent.right;y:2;text:tasks.equation;color:Theme.blueDeep;font.family:Theme.numberFont;font.pixelSize:11;font.weight:Font.DemiBold}
                        FitChart{anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.topMargin:34;anchors.bottom:legend.top;anchors.bottomMargin:6;rows:tasks.rows;slope:tasks.slope;intercept:tasks.intercept;showResiduals:root.showResiduals}
                        Row{id:legend;anchors.left:parent.left;anchors.bottom:parent.bottom;spacing:18
                            Row{spacing:6;Rectangle{width:10;height:10;radius:5;color:"white";border.width:2;border.color:Theme.blue}Text{text:"实测点";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}}
                            Row{spacing:6;Rectangle{width:14;height:2;color:Theme.blue;anchors.verticalCenter:parent.verticalCenter}Text{text:"最小二乘拟合";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}}
                            Row{visible:root.showResiduals;spacing:6;Rectangle{width:14;height:2;color:Theme.orange;anchors.verticalCenter:parent.verticalCenter}Text{text:"残差";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}}
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?94:120;spacing:root.compactLayout?7:10
                    MetricCard{Layout.fillWidth:true;Layout.fillHeight:true;label:"斜率";value:tasks.hasFit?tasks.slope.toFixed(root.compactLayout?4:5):"—";unit:"kPa/g";iconName:"trend";accent:Theme.blue}
                    MetricCard{Layout.fillWidth:true;Layout.fillHeight:true;label:"截距";value:tasks.hasFit?tasks.intercept.toFixed(3):"—";unit:"kPa";iconName:"target";accent:Theme.violet}
                    MetricCard{Layout.fillWidth:true;Layout.fillHeight:true;label:"相关系数 R";value:tasks.hasFit?tasks.pearsonR.toFixed(5):"—";iconName:"spark";accent:Theme.green}
                    MetricCard{Layout.fillWidth:true;Layout.fillHeight:true;label:"残差标准差";value:tasks.hasFit?tasks.residualStd.toFixed(3):"—";unit:"kPa";iconName:"pulse";accent:Theme.orange}
                }
            }

            ColumnLayout {
                Layout.preferredWidth:root.compactLayout?335:350;Layout.minimumWidth:Layout.preferredWidth;Layout.maximumWidth:Layout.preferredWidth
                Layout.fillHeight:true;spacing:root.compactLayout?8:12
                GlassPanel {
                    Layout.fillWidth:true;Layout.preferredHeight:root.compactLayout?200:300;radius:22
                    Item{anchors.fill:parent;anchors.margins:root.compactLayout?14:18
                        Text{text:"数据表";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.Bold}
                        Text{anchors.right:parent.right;text:"变量来源可追溯";color:Theme.green;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro;font.weight:Font.DemiBold}
                        Rectangle{id:dataHead;y:32;width:parent.width;height:29;radius:9;color:"#EDF4F9";Row{anchors.fill:parent;anchors.leftMargin:10;Text{width:45;anchors.verticalCenter:parent.verticalCenter;text:"#";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{width:100;anchors.verticalCenter:parent.verticalCenter;text:"质量 / g";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}Text{anchors.verticalCenter:parent.verticalCenter;text:"压力 / kPa";color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textMicro}}}
                        Flickable{anchors.left:parent.left;anchors.right:parent.right;anchors.top:dataHead.bottom;anchors.bottom:parent.bottom;clip:true;contentHeight:dataRows.height
                            boundsBehavior:Flickable.DragOverBounds;boundsMovement:Flickable.FollowBoundsBehavior;flickDeceleration:2200;maximumFlickVelocity:4200;pressDelay:80
                            Column{id:dataRows;width:parent.width;Repeater{model:tasks.rows;delegate:Rectangle{width:dataRows.width;height:35;color:index%2===0?"#35EAF4FB":"transparent";Row{anchors.fill:parent;anchors.leftMargin:10
                                Text{width:45;anchors.verticalCenter:parent.verticalCenter;text:modelData.index;color:Theme.inkFaint;font.family:Theme.numberFont;font.pixelSize: Theme.textMicro}
                                Text{width:100;anchors.verticalCenter:parent.verticalCenter;text:Number(modelData.mass).toFixed(2);color:Theme.ink;font.family:Theme.numberFont;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold}
                                Text{width:115;anchors.verticalCenter:parent.verticalCenter;text:Number(modelData.pressure).toFixed(3);color:Theme.ink;font.family:Theme.numberFont;font.pixelSize: Theme.textSmall;font.weight:Font.DemiBold}
                                Rectangle{anchors.verticalCenter:parent.verticalCenter;width:6;height:6;radius:3;color:Theme.green}
                            }}}}
                        }
                    }
                }
                GlassPanel {
                    Layout.fillWidth:true;Layout.fillHeight:true;radius:22
                    Item{anchors.fill:parent;anchors.margins:root.compactLayout?14:18
                        Row{spacing:8;VectorIcon{width:18;height:18;name:"shield";color:Theme.green}Text{text:"结果可信度";color:Theme.ink;font.family:Theme.fontFamily;font.pixelSize:13;font.weight:Font.Bold}}
                        Text{anchors.right:parent.right;text:tasks.containsEngineeringData?"工程结果":(tasks.rSquared>.99?"优秀":"需复核");color:tasks.containsEngineeringData?Theme.orange:(tasks.rSquared>.99?Theme.green:Theme.orange);font.family:Theme.fontFamily;font.pixelSize:11;font.weight:Font.Bold}
                        Column{y:root.compactLayout?31:38;width:parent.width;spacing:root.compactLayout?6:10
                            Repeater{model:[{t:"必填数据完整",ok:true},{t:"拟合点数不少于 3",ok:tasks.completedPoints>=3},{t:"R² 高于 0.99",ok:tasks.rSquared>.99},{t:tasks.containsEngineeringData?"工程数据：待标定验收":"协议与常温量值链已验收",ok:!tasks.containsEngineeringData&&(!device.hardwareMode||(deviceLink.protocolIntegrityAvailable&&device.valueTrustedForSafety))}];delegate:Row{width:parent.width;spacing:9
                                Rectangle{width:root.compactLayout?19:21;height:width;radius:7;color:modelData.ok?Theme.greenSoft:Theme.orangeSoft;VectorIcon{anchors.centerIn:parent;width:11;height:11;name:modelData.ok?"check":"info";color:modelData.ok?Theme.green:Theme.orange}}
                                Text{anchors.verticalCenter:parent.verticalCenter;text:modelData.t;color:Theme.inkMuted;font.family:Theme.fontFamily;font.pixelSize: Theme.textSmall}
                            }}
                        }
                        Rectangle{anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:reportButton.top;anchors.bottomMargin:12;height:1;color:Theme.lineSoft}
                        PremiumButton{id:reportButton;anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:48;compact:true;text:"生成任务报告";iconName:"file";variant:"primary";onClicked:app.showToast("报告计算与版式快照已生成（Demo）")}
                    }
                }
            }
        }
    }
}
