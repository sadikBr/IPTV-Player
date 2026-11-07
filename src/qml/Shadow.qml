import QtQuick

// Soft drop shadow behind a card. Put it before the card (so it is drawn
// underneath) and point `target` at the card.
BorderImage {
    property Item target
    property real spread: 22

    x: target ? target.x - spread : 0
    y: target ? target.y - spread + 4 : 0
    width: target ? target.width + 2 * spread : 0
    height: target ? target.height + 2 * spread : 0
    scale: target ? target.scale : 1
    visible: target ? target.visible : false
    source: "images/shadow.png"
    border { left: 34; top: 34; right: 34; bottom: 34 }
    horizontalTileMode: BorderImage.Stretch
    verticalTileMode: BorderImage.Stretch
    opacity: 0.8
}
