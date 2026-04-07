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

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            spacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true

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

        // State legend
        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                text: "States:"
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                font.bold: true
                opacity: 0.7
            }

            Row {
                spacing: 4
                Rectangle {
                    width: 8; height: 8; radius: 4; color: "#27ae60"
                    anchors.verticalCenter: parent.verticalCenter
                }
                QQC2.Label {
                    text: "Active (running)"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                }
            }

            Row {
                spacing: 4
                Rectangle {
                    width: 8; height: 8; radius: 4; color: "#e74c3c"
                    anchors.verticalCenter: parent.verticalCenter
                }
                QQC2.Label {
                    text: "Failed (error)"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                }
            }

            Row {
                spacing: 4
                Rectangle {
                    width: 8; height: 8; radius: 4; color: "#95a5a6"
                    anchors.verticalCenter: parent.verticalCenter
                }
                QQC2.Label {
                    text: "Inactive (stopped)"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                }
            }

            QQC2.Label {
                text: "|"
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.4
            }

            QQC2.Label {
                text: "enabled = starts at boot"
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
            }

            QQC2.Label {
                text: "disabled = manual start only"
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
            }

            QQC2.Label {
                text: "static = dependency only"
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
            }

            QQC2.Label {
                text: "masked = blocked from starting"
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
            }
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
                        QQC2.ToolTip.text: "Start this unit now"
                        QQC2.ToolTip.visible: hovered
                    }

                    // Stop button - visible when active
                    QQC2.ToolButton {
                        icon.name: "media-playback-stop"
                        visible: model.activeState === "active"
                        onClicked: kcm.stopUnit(model.name)
                        QQC2.ToolTip.text: "Stop this unit immediately"
                        QQC2.ToolTip.visible: hovered
                    }

                    // Restart button - always visible
                    QQC2.ToolButton {
                        icon.name: "view-refresh"
                        onClicked: kcm.restartUnit(model.name)
                        QQC2.ToolTip.text: "Stop and start this unit again"
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
                        QQC2.ToolTip.text: model.unitFileState === "enabled"
                                           ? "Disable: prevent starting automatically at boot"
                                           : "Enable: start this unit automatically at boot"
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
