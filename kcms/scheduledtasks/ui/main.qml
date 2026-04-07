// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    id: root

    header: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        QQC2.TabBar {
            id: tabBar
            Layout.fillWidth: true

            QQC2.TabButton {
                text: "systemd Timers"
                icon.name: "chronometer"
            }
            QQC2.TabButton {
                text: "Cron Jobs"
                icon.name: "document-edit"
            }
        }

        QQC2.Button {
            icon.name: "view-refresh"
            text: "Refresh"
            onClicked: kcm.refresh()
        }
    }

    StackLayout {
        currentIndex: tabBar.currentIndex
        anchors.fill: parent

        // ── Tab 1: systemd Timers ──
        ColumnLayout {
            spacing: 0

            ListView {
                id: timerList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: kcm.timers
                clip: true

                delegate: QQC2.ItemDelegate {
                    id: timerDelegate
                    width: timerList.width

                    required property var modelData
                    required property int index

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        // Status indicator
                        Rectangle {
                            width: Kirigami.Units.gridUnit * 0.75
                            height: Kirigami.Units.gridUnit * 0.75
                            radius: width / 2
                            color: timerDelegate.modelData.activeState === "active"
                                   ? "#27ae60" : "#95a5a6"
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Name, description, timing
                        ColumnLayout {
                            spacing: 0
                            Layout.fillWidth: true

                            QQC2.Label {
                                text: timerDelegate.modelData.name
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            QQC2.Label {
                                text: timerDelegate.modelData.description
                                opacity: 0.6
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                visible: text.length > 0
                            }

                            RowLayout {
                                spacing: Kirigami.Units.largeSpacing
                                visible: (timerDelegate.modelData.nextRun || "") !== ""
                                         || (timerDelegate.modelData.lastRun || "") !== ""

                                QQC2.Label {
                                    visible: (timerDelegate.modelData.nextRun || "") !== ""
                                    text: "Next: " + (timerDelegate.modelData.nextRun || "")
                                    opacity: 0.6
                                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                                }

                                QQC2.Label {
                                    visible: (timerDelegate.modelData.lastRun || "") !== ""
                                    text: "Last: " + (timerDelegate.modelData.lastRun || "")
                                    opacity: 0.6
                                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                                }
                            }
                        }

                        // Unit file state badge
                        QQC2.Label {
                            visible: (timerDelegate.modelData.unitFileState || "") !== ""
                            text: timerDelegate.modelData.unitFileState || ""
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            padding: Kirigami.Units.smallSpacing
                            background: Rectangle {
                                radius: 3
                                color: {
                                    let st = timerDelegate.modelData.unitFileState || "";
                                    if (st === "enabled") return Qt.rgba(0.15, 0.68, 0.38, 0.2);
                                    if (st === "disabled") return Qt.rgba(0.58, 0.65, 0.65, 0.2);
                                    if (st === "static") return Qt.rgba(0.20, 0.60, 0.86, 0.2);
                                    return "transparent";
                                }
                            }
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Action buttons
                        RowLayout {
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.ToolButton {
                                icon.name: "media-playback-start"
                                visible: timerDelegate.modelData.activeState !== "active"
                                onClicked: kcm.startTimer(timerDelegate.modelData.name)
                                QQC2.ToolTip.text: "Start"
                                QQC2.ToolTip.visible: hovered
                            }

                            QQC2.ToolButton {
                                icon.name: "media-playback-stop"
                                visible: timerDelegate.modelData.activeState === "active"
                                onClicked: kcm.stopTimer(timerDelegate.modelData.name)
                                QQC2.ToolTip.text: "Stop"
                                QQC2.ToolTip.visible: hovered
                            }

                            QQC2.ToolButton {
                                visible: {
                                    let st = timerDelegate.modelData.unitFileState || "";
                                    return st === "enabled" || st === "disabled";
                                }
                                icon.name: {
                                    let st = timerDelegate.modelData.unitFileState || "";
                                    return st === "enabled" ? "checkbox" : "checkbox-unchecked";
                                }
                                onClicked: {
                                    if (timerDelegate.modelData.unitFileState === "enabled") {
                                        kcm.disableTimer(timerDelegate.modelData.name);
                                    } else {
                                        kcm.enableTimer(timerDelegate.modelData.name);
                                    }
                                }
                                QQC2.ToolTip.text: {
                                    let st = timerDelegate.modelData.unitFileState || "";
                                    return st === "enabled" ? "Disable" : "Enable";
                                }
                                QQC2.ToolTip.visible: hovered
                            }
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: timerList.count === 0
                    text: "No timers found"
                    explanation: "No systemd timer units are currently loaded"
                }
            }
        }

        // ── Tab 2: Cron Jobs ──
        QQC2.ScrollView {
            ColumnLayout {
                width: parent.availableWidth
                spacing: Kirigami.Units.largeSpacing

                // User Crontab section
                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    Kirigami.Separator {
                        Kirigami.FormData.isSection: true
                        Kirigami.FormData.label: "User Crontab"
                    }

                    QQC2.TextArea {
                        id: crontabEditor
                        Kirigami.FormData.label: ""
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 12
                        font.family: "monospace"
                        wrapMode: TextEdit.Wrap
                        text: kcm.userCrontab
                        placeholderText: "No user crontab. Edit here to create one."
                        onTextChanged: kcm.userCrontab = text
                    }

                    RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Button {
                            text: "Save Crontab"
                            icon.name: "document-save"
                            onClicked: kcm.saveUserCrontab()
                        }

                        QQC2.Label {
                            text: "Saves via 'crontab -' (no root required)"
                            opacity: 0.6
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                        }
                    }
                }

                // System Cron Jobs section
                Kirigami.Separator {
                    Layout.fillWidth: true
                }

                QQC2.Label {
                    text: "System Cron Jobs (/etc/cron.d/)"
                    font.bold: true
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.1
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                }

                QQC2.Label {
                    visible: kcm.systemCronJobs.length === 0
                    text: "No files found in /etc/cron.d/"
                    opacity: 0.6
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                }

                Repeater {
                    model: kcm.systemCronJobs

                    ColumnLayout {
                        id: cronJobDelegate
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        Layout.rightMargin: Kirigami.Units.largeSpacing

                        required property var modelData
                        required property int index

                        property bool expanded: false

                        QQC2.ItemDelegate {
                            Layout.fillWidth: true
                            contentItem: RowLayout {
                                spacing: Kirigami.Units.smallSpacing

                                Kirigami.Icon {
                                    source: cronJobDelegate.expanded
                                            ? "arrow-down" : "arrow-right"
                                    implicitWidth: Kirigami.Units.iconSizes.small
                                    implicitHeight: Kirigami.Units.iconSizes.small
                                }

                                QQC2.Label {
                                    text: cronJobDelegate.modelData.filename
                                    font.bold: true
                                    Layout.fillWidth: true
                                }
                            }
                            onClicked: cronJobDelegate.expanded = !cronJobDelegate.expanded
                        }

                        QQC2.TextArea {
                            visible: cronJobDelegate.expanded
                            Layout.fillWidth: true
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 8
                            text: cronJobDelegate.modelData.content
                            readOnly: true
                            font.family: "monospace"
                            wrapMode: TextEdit.Wrap
                        }
                    }
                }

                // Bottom spacer
                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
