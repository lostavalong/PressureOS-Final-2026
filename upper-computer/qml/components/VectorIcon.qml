import QtQuick 2.15

Canvas {
    id: root
    property string name: "home"
    property color color: "#15395F"
    property real lineWidth: 1.7
    // Small status-bar glyphs need a little internal breathing room so the
    // antialiased stroke never touches the Canvas texture boundary.
    property real inset: 0
    antialiasing: true

    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onLineWidthChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onVisibleChanged: if (visible) requestPaint()
    Component.onCompleted: requestPaint()

    function strokeLine(ctx, x1, y1, x2, y2) {
        ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke()
    }
    function circle(ctx, x, y, r, fill) {
        ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2)
        if (fill) ctx.fill(); else ctx.stroke()
    }
    function poly(ctx, points, close) {
        ctx.beginPath(); ctx.moveTo(points[0], points[1])
        for (let i = 2; i < points.length; i += 2) ctx.lineTo(points[i], points[i + 1])
        if (close) ctx.closePath(); ctx.stroke()
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        ctx.save()
        const safeInset = Math.max(0, Math.min(inset, Math.min(width, height) / 3))
        ctx.translate(safeInset, safeInset)
        ctx.scale(Math.max(0, width - safeInset * 2) / 24,
                  Math.max(0, height - safeInset * 2) / 24)
        ctx.strokeStyle = color
        ctx.fillStyle = color
        ctx.lineWidth = lineWidth
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        switch (name) {
        case "home":
            poly(ctx, [3,11,12,3.5,21,11], false); poly(ctx, [5.5,9.5,5.5,20,18.5,20,18.5,9.5], false)
            strokeLine(ctx, 10,20,10,14); strokeLine(ctx,14,14,14,20); break
        case "gauge":
            ctx.beginPath(); ctx.arc(12,13,8,Math.PI,0); ctx.stroke(); strokeLine(ctx,5,18,19,18)
            strokeLine(ctx,12,13,16.2,9.2); circle(ctx,12,13,1.25,true); break
        case "task":
            poly(ctx,[6,4,19,4,19,20,6,20,6,4],true); strokeLine(ctx,9,9,16,9); strokeLine(ctx,9,13,16,13); strokeLine(ctx,9,17,13,17)
            ctx.beginPath(); ctx.moveTo(3.7,8.5);ctx.lineTo(4.6,9.4);ctx.lineTo(6.1,7.6);ctx.stroke(); break
        case "template": case "grid":
            for (let y=4;y<=14;y+=10) for(let x=4;x<=14;x+=10) { ctx.strokeRect(x,y,6,6) } break
        case "data": case "database":
            ctx.beginPath();ctx.ellipse(12,5.3,7,2.7,0,0,Math.PI*2);ctx.stroke();
            ctx.beginPath();ctx.moveTo(5,5.3);ctx.lineTo(5,18.3);ctx.bezierCurveTo(5,21.5,19,21.5,19,18.3);ctx.lineTo(19,5.3);ctx.stroke();
            ctx.beginPath();ctx.moveTo(5,11.7);ctx.bezierCurveTo(5,14.8,19,14.8,19,11.7);ctx.stroke(); break
        case "device":
            ctx.strokeRect(4,5,16,13); circle(ctx,12,20.5,0.7,true); strokeLine(ctx,9,8,15,8); circle(ctx,16.8,14.7,1,false); break
        case "assistant":
            ctx.beginPath();ctx.moveTo(5,5);ctx.quadraticCurveTo(3,5,3,8);ctx.lineTo(3,15);ctx.quadraticCurveTo(3,18,6,18);ctx.lineTo(8,18);ctx.lineTo(8,21);ctx.lineTo(12,18);ctx.lineTo(18,18);ctx.quadraticCurveTo(21,18,21,15);ctx.lineTo(21,8);ctx.quadraticCurveTo(21,5,18,5);ctx.closePath();ctx.stroke();
            circle(ctx,8,11.5,1,true);circle(ctx,12,11.5,1,true);circle(ctx,16,11.5,1,true); break
        case "arrow":
            strokeLine(ctx,4,12,20,12); poly(ctx,[15,7,20,12,15,17],false); break
        case "back":
            strokeLine(ctx,20,12,4,12); poly(ctx,[9,7,4,12,9,17],false); break
        case "check":
            poly(ctx,[4,12.5,9.2,17.5,20,6.5],false); break
        case "thermometer":
            ctx.beginPath();ctx.moveTo(10,14.2);ctx.lineTo(10,5.5);ctx.quadraticCurveTo(10,3,12,3);ctx.quadraticCurveTo(14,3,14,5.5);ctx.lineTo(14,14.2);ctx.stroke();circle(ctx,12,17,4,false);strokeLine(ctx,12,8,12,17); break
        case "pulse":
            poly(ctx,[2,12,7,12,9,7,12.5,17,15,10,17,12,22,12],false); break
        case "clock":
            circle(ctx,12,12,8,false); strokeLine(ctx,12,12,12,7); strokeLine(ctx,12,12,16,14); break
        case "chart": case "trend":
            strokeLine(ctx,4,3.5,4,20);strokeLine(ctx,4,20,21,20);poly(ctx,[7,16,11,12,14,14,20,7],false);circle(ctx,20,7,1,true); break
        case "target":
            circle(ctx,12,12,8,false);circle(ctx,12,12,4,false);circle(ctx,12,12,1.3,true);strokeLine(ctx,12,2,12,5);strokeLine(ctx,19,12,22,12); break
        case "flask":
            strokeLine(ctx,9,3,15,3);strokeLine(ctx,10,3,10,9);strokeLine(ctx,14,3,14,9);
            ctx.beginPath();ctx.moveTo(10,9);ctx.lineTo(5,18);ctx.quadraticCurveTo(4,21,8,21);ctx.lineTo(16,21);ctx.quadraticCurveTo(20,21,19,18);ctx.lineTo(14,9);ctx.stroke();strokeLine(ctx,7.5,16,16.5,16); break
        case "shield":
            ctx.beginPath();ctx.moveTo(12,3);ctx.lineTo(20,6);ctx.lineTo(19,13);ctx.quadraticCurveTo(18,19,12,21);ctx.quadraticCurveTo(6,19,5,13);ctx.lineTo(4,6);ctx.closePath();ctx.stroke();poly(ctx,[8.5,12,11,14.5,16,9.5],false); break
        case "file":
            poly(ctx,[6,3,15,3,20,8,20,21,6,21,6,3],true);poly(ctx,[15,3,15,8,20,8],false);strokeLine(ctx,9,13,17,13);strokeLine(ctx,9,17,15,17); break
        case "wifi":
            ctx.beginPath();ctx.arc(12,18,14,-2.28,-0.86);ctx.stroke();ctx.beginPath();ctx.arc(12,18,9,-2.28,-0.86);ctx.stroke();ctx.beginPath();ctx.arc(12,18,4,-2.28,-0.86);ctx.stroke();circle(ctx,12,18,1,true); break
        case "bluetooth":
            // Standard Bluetooth bind-rune: two matching upper/lower wedges
            // sharing one vertical spine (equivalent to the familiar logo).
            poly(ctx,[7,7,17,17,12,22,12,2,17,7,7,17],false); break
        case "usb":
            strokeLine(ctx,12,4,12,17);poly(ctx,[9,7,12,4,15,7],false);strokeLine(ctx,12,11,7,11);strokeLine(ctx,7,11,7,8);ctx.strokeRect(5.5,6.5,3,2);strokeLine(ctx,12,14,17,14);circle(ctx,18.5,14,1.5,false);circle(ctx,12,19,2,false); break
        case "phone":
            ctx.strokeRect(7,2.5,10,19);strokeLine(ctx,10,5,14,5);circle(ctx,12,18.5,0.8,true); break
        case "settings":
            circle(ctx,12,12,3,false);circle(ctx,12,12,8,false);strokeLine(ctx,12,2,12,4);strokeLine(ctx,12,20,12,22);strokeLine(ctx,2,12,4,12);strokeLine(ctx,20,12,22,12);strokeLine(ctx,5,5,6.5,6.5);strokeLine(ctx,17.5,17.5,19,19); break
        case "power":
            ctx.beginPath();ctx.arc(12,13,8,-Math.PI*0.25,Math.PI*1.25);ctx.stroke();strokeLine(ctx,12,2.5,12,12); break
        case "record":
            circle(ctx,12,12,7,false);circle(ctx,12,12,3,true); break
        case "zero":
            ctx.beginPath();ctx.arc(12,12,8,-2.5,2.0);ctx.stroke();poly(ctx,[3.5,7,4.5,13,9.5,9.5],false); break
        case "filter":
            poly(ctx,[3,5,21,5,14,13,14,19,10,21,10,13,3,5],false); break
        case "export":
            ctx.strokeRect(4,8,16,13);strokeLine(ctx,12,16,12,3);poly(ctx,[8,7,12,3,16,7],false); break
        case "play":
            ctx.beginPath();ctx.moveTo(8,5);ctx.lineTo(19,12);ctx.lineTo(8,19);ctx.closePath();ctx.stroke(); break
        case "search":
            circle(ctx,10,10,6,false);strokeLine(ctx,14.5,14.5,21,21); break
        case "sync":
            ctx.beginPath();ctx.arc(12,12,8,-2.7,0.5);ctx.stroke();poly(ctx,[18,5,20,10,15,9],false);ctx.beginPath();ctx.arc(12,12,8,0.45,3.6);ctx.stroke();poly(ctx,[6,19,4,14,9,15],false); break
        case "expand":
            poly(ctx,[9,4,4,4,4,9],false);poly(ctx,[15,4,20,4,20,9],false);
            poly(ctx,[4,15,4,20,9,20],false);poly(ctx,[20,15,20,20,15,20],false); break
        case "close":
            strokeLine(ctx,5,5,19,19);strokeLine(ctx,19,5,5,19); break
        case "delete":
            poly(ctx,[7,7,17,7,16,21,8,21,7,7],false);strokeLine(ctx,5,7,19,7);strokeLine(ctx,9,4,15,4);strokeLine(ctx,10,10,10.5,18);strokeLine(ctx,14,10,13.5,18); break
        case "warning":
            ctx.beginPath();ctx.moveTo(12,3);ctx.lineTo(22,21);ctx.lineTo(2,21);ctx.closePath();ctx.stroke();strokeLine(ctx,12,9,12,15);circle(ctx,12,18,1,true); break
        case "edit":
            poly(ctx,[5,18,6,14,16,4,20,8,10,18,5,18],false);strokeLine(ctx,14.5,5.5,18.5,9.5);strokeLine(ctx,4,21,20,21); break
        case "info":
            circle(ctx,12,12,9,false);circle(ctx,12,7.5,1,true);strokeLine(ctx,12,11,12,17); break
        case "spark":
            poly(ctx,[12,2,14,9.5,22,12,14,14.5,12,22,10,14.5,2,12,10,9.5,12,2],true); break
        case "lock":
            ctx.strokeRect(5,10,14,11);ctx.beginPath();ctx.arc(12,10,5,-Math.PI,0);ctx.stroke();circle(ctx,12,15,1,true);strokeLine(ctx,12,16,12,18); break
        case "more":
            circle(ctx,5,12,1.3,true);circle(ctx,12,12,1.3,true);circle(ctx,19,12,1.3,true); break
        default:
            circle(ctx,12,12,8,false);circle(ctx,12,12,2,true)
        }
        ctx.restore()
    }
}
