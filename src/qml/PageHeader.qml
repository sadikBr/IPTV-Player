import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import IPTVPlayer

// Title row at the top of each section: optional back button, title,
// anything placed inside (search, actions) and, on phones, the provider.
// When the actions don't fit next to the title they get a row of their own
// below it (scrollable if even that is too narrow), so none is cut off.
Item {
    id: root

    property string title
    property string subtitle
    property bool showBack: false
    property bool narrow: width < AppTheme.compactWidth
    default property alias actions: actionRow.data

    signal backClicked()

    readonly property real titleHeight: narrow ? 64 : 76
    readonly property real sideMargin: narrow ? 8 : AppTheme.pageMargin
    // Room next to the title for the actions.
    readonly property real spaceForActions: width - 2 * sideMargin
                                            - (backButton.visible ? backButton.implicitWidth + AppTheme.spacing : 0)
                                            - Math.min(titleLabel.implicitWidth, width * 0.5) - 8
                                            - (provider.visible ? provider.implicitWidth + AppTheme.spacing : 0)
                                            - 2 * AppTheme.spacing
    readonly property bool wrapped: actionRow.implicitWidth > spaceForActions

    implicitHeight: titleHeight + (wrapped ? 52 : 0)

    RowLayout {
        id: titleRow
        width: parent.width
        height: root.titleHeight
        anchors.leftMargin: root.sideMargin
        anchors.rightMargin: root.sideMargin
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: AppTheme.spacing

        IconButton {
            id: backButton
            visible: root.showBack
            iconName: "back"
            tip: qsTr("Back")
            onClicked: root.backClicked()
        }

        ColumnLayout {
            spacing: 0
            Layout.leftMargin: root.showBack || !root.narrow ? 0 : 8
            Label {
                id: titleLabel
                text: root.title
                font.pixelSize: root.narrow ? 22 : AppTheme.fontH1
                font.weight: Font.Bold
                color: AppTheme.textPrimary
                elide: Text.ElideRight
                Layout.maximumWidth: root.width * 0.5
            }
            Label {
                visible: text.length > 0 && !root.narrow
                text: root.subtitle
                font.pixelSize: AppTheme.fontSmall
                color: AppTheme.textTertiary
            }
        }

        Item { Layout.fillWidth: true }

        // The actions sit here when they fit.
        Item {
            id: inlineSlot
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            implicitWidth: root.wrapped ? 0 : actionRow.implicitWidth
            implicitHeight: actionRow.implicitHeight
        }

        ProviderSwitcher {
            id: provider
            visible: root.narrow && AppState.hasSource
            compact: true
        }
    }

    // ...otherwise on a row of their own.
    Flickable {
        id: wrapRow
        visible: root.wrapped
        anchors.top: titleRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.sideMargin
        anchors.rightMargin: root.sideMargin
        height: 52
        contentWidth: Math.max(width, actionRow.implicitWidth)
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Item {
            id: wrapSlot
            width: wrapRow.contentWidth
            height: wrapRow.height
        }
    }

    RowLayout {
        id: actionRow
        parent: root.wrapped ? wrapSlot : inlineSlot
        // Right-aligned like the inline actions.
        x: parent ? Math.max(0, parent.width - implicitWidth) : 0
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
        width: implicitWidth
        spacing: 6
    }
}
