import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts
import IPTVPlayer

// One episode: 16:9 still with progress, number, title, length and plot.
// compact: still on the left, text on the right (phones).
ItemDelegate {
    id: root

    required property string extId
    required property string title
    required property int episode
    required property string plot
    required property string image
    required property int durationSecs
    required property real watched

    property bool compact: false
    readonly property bool emphasized: hovered || visualFocus

    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    background: Item {}

    contentItem: GridLayout {
        columns: root.compact ? 2 : 1
        columnSpacing: 14
        rowSpacing: 10

        Item {
            Layout.preferredWidth: root.compact ? 136 : -1
            Layout.fillWidth: !root.compact
            Layout.preferredHeight: (root.compact ? 136 : root.width) * 9 / 16
            Layout.alignment: Qt.AlignTop

            Shadow { target: still; opacity: root.emphasized ? 1 : 0.5 }
            FocusRing { target: root; gap: root.compact ? 3 : 7; radius: AppTheme.radius + gap }
            Rectangle {
                id: still
                anchors.fill: parent
                radius: AppTheme.radius
                color: AppTheme.surfaceHigh
                clip: true
                scale: root.emphasized && !root.compact ? 1.03 : 1
                Behavior on scale { NumberAnimation { duration: AppTheme.normal; easing.type: Easing.OutCubic } }

                ArtworkPlaceholder {
                    anchors.fill: parent
                    visible: !AppTheme.hasArtwork(thumb)
                    title: root.title
                    showTitle: false
                    iconName: "series"
                }
                Label {
                    anchors.centerIn: parent
                    visible: !AppTheme.hasArtwork(thumb)
                    text: root.episode
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    color: AppTheme.textTertiary
                }
                Image {
                    id: thumb
                    anchors.fill: parent
                    source: root.image
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize.width: 480
                    visible: AppTheme.hasArtwork(thumb)
                }
                Rectangle {
                    anchors.fill: parent
                    color: "#73000000"
                    opacity: root.emphasized ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: AppTheme.normal } }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 46
                        height: 46
                        radius: 23
                        color: "#E6FFFFFF"
                        IconImage {
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: 2
                            source: "icons/play.svg"
                            sourceSize: Qt.size(22, 22)
                            color: "#0B0B0D"
                        }
                    }
                }
                Rectangle {
                    visible: root.watched > 0
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 4
                    color: "#66000000"
                    Rectangle {
                        width: parent.width * Math.min(1, root.watched)
                        height: parent.height
                        color: AppTheme.accent
                    }
                }
                // Watched badge
                Rectangle {
                    visible: root.watched >= 1
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: 8
                    width: 22
                    height: 22
                    radius: 11
                    color: "#CC000000"
                    IconImage {
                        anchors.centerIn: parent
                        source: "icons/check.svg"
                        sourceSize: Qt.size(13, 13)
                        color: AppTheme.success
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 4
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    text: root.episode + ". " + root.title
                    elide: Text.ElideRight
                    color: root.emphasized ? "white" : AppTheme.textPrimary
                    font.pixelSize: AppTheme.fontBody
                    font.weight: Font.DemiBold
                }
                Label {
                    visible: root.durationSecs > 0
                    text: Math.round(root.durationSecs / 60) + " min"
                    color: AppTheme.textTertiary
                    font.pixelSize: AppTheme.fontCaption
                }
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.plot
                wrapMode: Text.Wrap
                maximumLineCount: root.compact ? 2 : 3
                elide: Text.ElideRight
                color: AppTheme.textSecondary
                font.pixelSize: AppTheme.fontSmall
                lineHeight: 1.2
            }
        }
    }
}
