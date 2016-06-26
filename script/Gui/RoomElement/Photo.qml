import QtQuick 2.4
import QtQuick.Layouts 1.1
import QtGraphicalEffects 1.0
import Cardirector.Device 1.0
import Cardirector.Resource 1.0

Item {
    property string headGeneral: ""
    property string deputyGeneral: ""
    property alias screenName: screenNameItem.text
    property alias faceTurned: faceTurnedCover.visible
    property string userRole: "unknown"
    property string kingdom: "unknown"
    property alias handcardNum: handcardNumItem.value
    property alias maxHp: hpBar.maxValue
    property alias hp: hpBar.value
    property alias handcardArea: handcardAreaItem
    property alias equipArea: equipAreaItem
    property alias delayedTrickArea: delayedTrickAreaItem
    property string phase: "inactive"
    property bool chained: false
    property bool dying: false
    property bool alive: true
    property bool drunk: false
    property alias progressBar: progressBarItem
    property int seat: 0
    property bool selectable: false
    property bool selected: false

    id: root
    width: Device.gu(157)
    height: Device.gu(181)
    states: [
        State {
            name: "normal"
            PropertyChanges {
                target: outerGlow
                visible: false
            }
        },
        State {
            name: "candidate"
            PropertyChanges {
                target: outerGlow
                color: "#EEB300"
                visible: root.selectable && root.selected
            }
        },
        State {
            name: "playing"
            PropertyChanges {
                target: outerGlow
                color: "#BE85EE"
                visible: true
            }
        },
        State {
            name: "responding"
            PropertyChanges {
                target: outerGlow
                color: "#51D659"
                visible: true
            }
        },
        State {
            name: "sos"
            PropertyChanges {
                target: outerGlow
                color: "#ED8B96"
                visible: true
            }
        }
    ]
    state: "normal"

    RectangularGlow {
        id: outerGlow
        anchors.fill: parent
        visible: true
        glowRadius: 8
        spread: 0.4
        cornerRadius: 8
    }

    Item {
        width: deputyGeneral != "" ? Device.gu(75) : Device.gu(155)
        height: Device.gu(182)

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            source: "image://root/general/fullphoto/" + (headGeneral != "" ? headGeneral : "blank")
        }
    }

    Item {
        x: Device.gu(80)
        width: Device.gu(75)
        height: Device.gu(182)

        Image {
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            source: deputyGeneral != "" ? "image://root/general/fullphoto/" + deputyGeneral : ""
        }
    }

    Rectangle {
        color: Qt.rgba(250, 0, 0, 0.45)
        anchors.fill: parent
        visible: parent.drunk
    }

    Image {
        source: "image://root/photo/circle-photo"
        visible: deputyGeneral != ""
    }

    Image {
        id: faceTurnedCover
        anchors.fill: parent
        source: "image://root/photo/faceturned"
        visible: false
    }

    Image {
        anchors.fill: parent
        source: "image://root/photo/photo-back"
    }

    SimpleEquipArea {
        id: equipAreaItem

        width: parent.width - Device.gu(20)
        height: Device.gu(60)
        y: parent.height - height
    }

    HandcardNumber {
        id: handcardNumItem
        x: Device.gu(-10)
        y: Device.gu(102)
        kingdom: parent.kingdom
        value: handcardArea.length
    }

    Item {
        width: Device.gu(17)
        height: Device.gu(maxHp > 5 ? 72 : 6 + 15 * maxHp)
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Device.gu(1)
        anchors.rightMargin: Device.gu(2)
        clip: true

        Image {
            source: "image://root/magatama/bg"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            visible: hpBar.visible
        }

        HpBar {
            id: hpBar
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Device.gu(3)
            visible: maxHp > 0

            transform: Scale {
                origin.x: hpBar.width / 2
                origin.y: hpBar.height
                xScale: Device.gu(15) / hpBar.width
                yScale: xScale
            }
        }
    }

    Text {
        id: screenNameItem
        color: "white"
        font.pixelSize: Device.gu(12)
        anchors.left: parent.left
        anchors.leftMargin: Device.gu(20)
        anchors.right: parent.right
        anchors.rightMargin: Device.gu(35)
        horizontalAlignment: Text.AlignHCenter
        y: Device.gu(3)
    }

    GlowText {
        id: headGeneralNameItem
        color: "white"
        y: Device.gu(30)
        font.pixelSize: Device.gu(18)
        font.family: "LiSu"
        font.weight: Font.Bold
        width: Device.gu(32)
        wrapMode: Text.WrapAnywhere
        lineHeight: 1.5
        horizontalAlignment: Text.AlignHCenter
        text: qsTr(headGeneral)

        glow.color: "black"
        glow.spread: 0.7
        glow.radius: Device.gu(6)
        glow.samples: 24
    }

    GlowText {
        id: deputyGeneralNameItem
        color: "white"
        x: Device.gu(80)
        y: Device.gu(30)
        font.pixelSize: Device.gu(18)
        font.family: "LiSu"
        font.weight: Font.Bold
        width: Device.gu(32)
        wrapMode: Text.WrapAnywhere
        lineHeight: 1.5
        horizontalAlignment: Text.AlignHCenter
        text: qsTr(deputyGeneral)

        glow.color: "black"
        glow.spread: 0.7
        glow.radius: Device.gu(6)
        glow.samples: 24
    }

    Image {
        source: "image://root/chain"
        anchors.centerIn: parent
        visible: parent.chained
    }

    Image {
        source: "image://root/photo/save-me"
        anchors.centerIn: parent
        visible: parent.dying
    }

    Rectangle {
        id: disableMask
        anchors.fill: parent
        color: "black"
        opacity: 0.3
        visible: root.state == "candidate" && !root.selectable
    }

    DelayedTrickArea {
        id: delayedTrickAreaItem
        columns: 1
        x: -Device.gu(15)
        y: Device.gu(18)
    }

    Image {
        source: root.phase != "inactive" ? "image://root/phase/" + root.phase + ".png" : ""
        width: parent.width * 0.9
        height: implicitHeight / implicitWidth * width
        x: (parent.width - width) / 2
        y: parent.height - Device.gu(3)
        visible: root.phase != "inactive"
    }

    Colorize {
        anchors.fill: parent
        source: parent
        hue: 0
        saturation: 0
        lightness: 0
        visible: !parent.alive
    }

    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (parent.state != "candidate" || !parent.selectable)
                return;
            parent.selected = !parent.selected;
        }
    }

    KingdomBox {
        x: parent.width - width - Device.gu(4)
        y: Device.gu(4)
        value: parent.kingdom
    }

    RoleComboBox {
        x: Device.gu(3)
        y: Device.gu(2)
        value: parent.userRole
    }


    ProgressBar {
        id: progressBarItem
        width: parent.width
        height: 10
        y: parent.height + Device.gu(10)
        visible: false
    }

    InvisibleCardArea {
        id: handcardAreaItem
        anchors.centerIn: parent
    }

    InvisibleCardArea {
        id: defaultArea
        anchors.centerIn: parent
    }

    function add(inputs)
    {
        defaultArea.add(inputs);
    }

    function remove(outputs)
    {
        return defaultArea.remove(outputs);
    }

    function updateCardPosition(animated)
    {
        defaultArea.updateCardPosition(animated);
    }
}
