// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.ScrollViewKCM {
    id: root

    header: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            spacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true

            Kirigami.SearchField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Search log messages..."
                onAccepted: kcm.journalModel.searchText = text
            }

            QQC2.ComboBox {
                id: priorityCombo
                model: ["Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug"]
                currentIndex: kcm.journalModel.priorityFilter
                onCurrentIndexChanged: kcm.journalModel.priorityFilter = currentIndex

                QQC2.ToolTip.text: {
                    var descriptions = [
                        "Emergency (0): System is unusable",
                        "Alert (1): Immediate action required",
                        "Critical (2): Critical conditions",
                        "Error (3): Error conditions",
                        "Warning (4): Warning conditions",
                        "Notice (5): Normal but significant",
                        "Info (6): Informational",
                        "Debug (7): Debug messages"
                    ];
                    return "Show messages up to: " + descriptions[currentIndex];
                }
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: 500
            }

            QQC2.CheckBox {
                id: kernelCheck
                text: "Kernel only"
                checked: kcm.journalModel.kernelOnly
                onCheckedChanged: kcm.journalModel.kernelOnly = checked

                QQC2.ToolTip.text: "Show only kernel ring buffer messages (equivalent to dmesg)"
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: 500
            }

            QQC2.Button {
                icon.name: "view-refresh"
                text: "Refresh"
                onClicked: kcm.journalModel.refresh()
            }
        }

        // Priority legend
        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.smallSpacing
            Layout.rightMargin: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                text: "Priorities:"
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                font.bold: true
                opacity: 0.7
            }

            Row {
                spacing: 4
                Rectangle {
                    width: 8; height: 8; radius: 4; color: "#e74c3c"
                    anchors.verticalCenter: parent.verticalCenter
                }
                QQC2.Label {
                    text: "Emergency/Alert/Critical"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                }
            }

            Row {
                spacing: 4
                Rectangle {
                    width: 8; height: 8; radius: 4; color: "#e67e22"
                    anchors.verticalCenter: parent.verticalCenter
                }
                QQC2.Label {
                    text: "Error"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                }
            }

            Row {
                spacing: 4
                Rectangle {
                    width: 8; height: 8; radius: 4; color: "#f1c40f"
                    anchors.verticalCenter: parent.verticalCenter
                }
                QQC2.Label {
                    text: "Warning"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                }
            }

            QQC2.Label {
                text: "Notice/Info/Debug (no indicator)"
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
            }
        }
    }

    view: ListView {
        id: journalList
        model: kcm.journalModel
        clip: true

        delegate: QQC2.ItemDelegate {
            width: journalList.width

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                // Priority color badge
                Rectangle {
                    width: Kirigami.Units.gridUnit * 0.75
                    height: Kirigami.Units.gridUnit * 0.75
                    radius: width / 2
                    color: {
                        if (model.priority <= 2) return "#e74c3c";       // red: EMERG/ALERT/CRIT
                        if (model.priority === 3) return "#e67e22";      // orange: ERR
                        if (model.priority === 4) return "#f1c40f";      // yellow: WARNING
                        return "transparent";
                    }
                    visible: model.priority <= 4
                    Layout.alignment: Qt.AlignVCenter
                }

                // Spacer when no badge
                Item {
                    width: Kirigami.Units.gridUnit * 0.75
                    height: Kirigami.Units.gridUnit * 0.75
                    visible: model.priority > 4
                    Layout.alignment: Qt.AlignVCenter
                }

                // Timestamp
                QQC2.Label {
                    text: model.timestamp
                    font.family: "monospace"
                    opacity: 0.7
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 11
                }

                // Priority name
                QQC2.Label {
                    text: model.priorityName
                    font.bold: model.priority <= 3
                    color: {
                        if (model.priority <= 2) return "#e74c3c";
                        if (model.priority === 3) return "#e67e22";
                        if (model.priority === 4) return "#f1c40f";
                        return Kirigami.Theme.textColor;
                    }
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                }

                // Unit name
                QQC2.Label {
                    text: model.unit || ""
                    opacity: 0.6
                    elide: Text.ElideRight
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                }

                // Message
                QQC2.Label {
                    text: model.message
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: journalList.count === 0
            text: "No log entries found"
            explanation: "Try adjusting the priority filter or clearing the search"
        }
    }
}
