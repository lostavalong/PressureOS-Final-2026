import QtQuick 2.15
import PressureOS 1.0

Canvas {
    id: root
    property var rows: []
    property real slope: 0
    property real intercept: 0
    property bool showResiduals: false
    antialiasing: true
    onRowsChanged: requestPaint()
    onSlopeChanged: requestPaint()
    onInterceptChanged: requestPaint()
    onShowResidualsChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const ctx=getContext("2d");ctx.clearRect(0,0,width,height)
        const left=48,right=18,top=18,bottom=34,w=width-left-right,h=height-top-bottom
        ctx.strokeStyle=Theme.lineSoft;ctx.lineWidth=1;ctx.setLineDash([4,6]);ctx.font="10px '"+Theme.fontFamily+"'";ctx.fillStyle=Theme.inkFaint
        for(let i=0;i<5;++i){let y=top+i/4*h;ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(width-right,y);ctx.stroke()}
        for(let j=0;j<6;++j){let x=left+j/5*w;ctx.beginPath();ctx.moveTo(x,top);ctx.lineTo(x,height-bottom);ctx.stroke()}
        ctx.setLineDash([])
        if(!rows||rows.length===0)return
        let minX=Number(rows[0].mass),maxX=minX,minY=Number(rows[0].pressure),maxY=minY
        for(let n=0;n<rows.length;++n){minX=Math.min(minX,Number(rows[n].mass));maxX=Math.max(maxX,Number(rows[n].mass));minY=Math.min(minY,Number(rows[n].pressure));maxY=Math.max(maxY,Number(rows[n].pressure))}
        let spanX=Math.max(1,maxX-minX),spanY=Math.max(1,maxY-minY);minX-=spanX*.14;maxX+=spanX*.14;minY-=spanY*.18;maxY+=spanY*.18;spanX=maxX-minX;spanY=maxY-minY
        function px(x){return left+(x-minX)/spanX*w} function py(y){return top+h-(y-minY)/spanY*h}
        ctx.strokeStyle=Theme.blue;ctx.lineWidth=2.3;ctx.beginPath();ctx.moveTo(px(minX),py(slope*minX+intercept));ctx.lineTo(px(maxX),py(slope*maxX+intercept));ctx.stroke()
        for(let p=0;p<rows.length;++p){const x=Number(rows[p].mass),y=Number(rows[p].pressure),pred=slope*x+intercept
            if(showResiduals){ctx.strokeStyle="#F19A45";ctx.lineWidth=1.3;ctx.setLineDash([3,3]);ctx.beginPath();ctx.moveTo(px(x),py(pred));ctx.lineTo(px(x),py(y));ctx.stroke();ctx.setLineDash([])}
            ctx.fillStyle="white";ctx.strokeStyle=Theme.blue;ctx.lineWidth=2.4;ctx.beginPath();ctx.arc(px(x),py(y),5,0,Math.PI*2);ctx.fill();ctx.stroke()
        }
        ctx.fillStyle=Theme.inkFaint;ctx.font="10px '"+Theme.fontFamily+"'";ctx.fillText("质量 / g",width-58,height-8);ctx.save();ctx.translate(12,54);ctx.rotate(-Math.PI/2);ctx.fillText("压力 / kPa",0,0);ctx.restore()
    }
}
