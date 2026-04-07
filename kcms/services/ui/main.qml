// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.ScrollViewKCM {
    id: root

    readonly property var typeFilterValues: ["", "service", "timer", "socket", "mount", "target"]

    header: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.SearchField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "Filter units..."
            onTextChanged: kcm.unitModel.searchText = text
        }

        QQC2.ComboBox {
            id: typeCombo
            model: ["All", "Services", "Timers", "Sockets", "Mounts", "Targets"]
            onCurrentIndexChanged: kcm.unitModel.typeFilter = root.typeFilterValues[currentIndex]
        }

        QQC2.CheckBox {
            id: userCheck
            text: "User Services"
            checked: kcm.unitModel.showUserUnits
            onCheckedChanged: kcm.unitModel.showUserUnits = checked
        }

        QQC2.Button {
            icon.name: "view-refresh"
            text: "Refresh"
            onClicked: kcm.unitModel.refresh()
        }
    }

    view: ListView {
        id: unitList
        model: kcm.unitModel
        clip: true

        delegate: QQC2.ItemDelegate {
            width: unitList.width

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                // Status indicator circle
                Rectangle {
                    width: Kirigami.Units.gridUnit * 0.75
                    height: Kirigami.Units.gridUnit * 0.75
                    radius: width / 2
                    color: {
                        if (model.activeState === "active") return "#27ae60";
                        if (model.activeState === "failed") return "#e74c3c";
                        return "#95a5a6";
                    }
                    Layout.alignment: Qt.AlignVCenter
                }

                // Unit name and description
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true

                    QQC2.Label {
                        text: model.name
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    QQC2.Label {
                        text: model.description
                        opacity: 0.6
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        visible: text.length > 0
                    }
                }

                // Unit file state badge
                QQC2.Label {
                    visible: model.unitFileState !== ""
                    text: model.unitFileState
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    padding: Kirigami.Units.smallSpacing
                    background: Rectangle {
                        radius: 3
                        color: {
                            if (model.unitFileState === "enabled") return Qt.rgba(0.15, 0.68, 0.38, 0.2);
                            if (model.unitFileState === "disabled") return Qt.rgba(0.58, 0.65, 0.65, 0.2);
                            if (model.unitFileState === "masked") return Qt.rgba(0.91, 0.30, 0.24, 0.2);
                            if (model.unitFileState === "static") return Qt.rgba(0.20, 0.60, 0.86, 0.2);
                            return "transparent";
                        }
                    }
                    Layout.alignment: Qt.AlignVCenter
                }

                // Action buttons
                RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    // Start button - visible when not active
                    QQC2.ToolButton {
                        icon.name: "media-playback-start"
                        visible: model.activeState !== "active"
                        onClicked: kcm.startUnit(model.name)
                        QQC2.ToolTip.text: "Start"
                        QQC2.ToolTip.visible: hovered
                    }

                    // Stop button - visible when active
                    QQC2.ToolButton {
                        icon.name: "media-playback-stop"
                        visible: model.activeState === "active"
                        onClicked: kcm.stopUnit(model.name)
                        QQC2.ToolTip.text: "Stop"
                        QQC2.ToolTip.visible: hovered
                    }

                    // Restart button - always visible
                    QQC2.ToolButton {
                        icon.name: "view-refresh"
                        onClicked: kcm.restartUnit(model.name)
                        QQC2.ToolTip.text: "Restart"
                        QQC2.ToolTip.visible: hovered
                    }

                    // Enable/disable toggle
                    QQC2.ToolButton {
                        visible: model.unitFileState === "enabled" || model.unitFileState === "disabled"
                        icon.name: model.unitFileState === "enabled" ? "checkbox" : "checkbox-unchecked"
                        onClicked: {
                            if (model.unitFileState === "enabled") {
                                kcm.disableUnit(model.name);
                            } else {
                                kcm.enableUnit(model.name);
                            }
                        }
                        QQC2.ToolTip.text: model.unitFileState === "enabled" ? "Disable" : "Enable"
                        QQC2.ToolTip.visible: hovered
                    }
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: unitList.count === 0
            text: "No units found"
            explanation: "Try adjusting the type filter or clearing the search"
        }
    }
}
