// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    id: root

    header: QQC2.TabBar {
        id: tabBar

        QQC2.TabButton {
            text: "GRUB Settings"
        }
        QQC2.TabButton {
            text: "Kernel Entries"
        }
    }

    StackLayout {
        currentIndex: tabBar.currentIndex

        // ── Tab 1: GRUB Settings ──
        Kirigami.FormLayout {
            QQC2.TextField {
                Kirigami.FormData.label: "Timeout (seconds):"
                text: kcm.grubTimeout
                onTextEdited: kcm.grubTimeout = text
            }

            QQC2.Label {
                text: "Time in seconds the boot menu is shown before auto-selecting the default entry. Set to 0 to skip the menu (boot immediately), -1 to wait indefinitely."
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
            }

            QQC2.TextField {
                Kirigami.FormData.label: "Default Entry:"
                text: kcm.grubDefault
                onTextEdited: kcm.grubDefault = text
            }

            QQC2.Label {
                text: "Which entry to boot by default. Can be a number (0 = first entry), 'saved' (remembers last choice), or a menu entry title."
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
            }

            QQC2.ScrollView {
                Kirigami.FormData.label: "Kernel Parameters (DEFAULT):"
                Layout.fillWidth: true
                implicitHeight: Kirigami.Units.gridUnit * 3

                QQC2.TextArea {
                    text: kcm.grubCmdlineDefault
                    font.family: "monospace"
                    wrapMode: TextEdit.Wrap
                    onTextChanged: {
                        if (text !== kcm.grubCmdlineDefault) {
                            kcm.grubCmdlineDefault = text
                        }
                    }
                }
            }

            QQC2.Label {
                text: "Kernel parameters for the default boot entry only. Common values: 'quiet' (suppress boot messages), 'splash' (show boot splash), 'resume=...' (hibernation resume device), 'mitigations=off' (disable CPU vulnerability mitigations, faster but less secure)."
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
            }

            QQC2.ScrollView {
                Kirigami.FormData.label: "Kernel Parameters (ALL):"
                Layout.fillWidth: true
                implicitHeight: Kirigami.Units.gridUnit * 3

                QQC2.TextArea {
                    text: kcm.grubCmdlineLinux
                    font.family: "monospace"
                    wrapMode: TextEdit.Wrap
                    onTextChanged: {
                        if (text !== kcm.grubCmdlineLinux) {
                            kcm.grubCmdlineLinux = text
                        }
                    }
                }
            }

            QQC2.Label {
                text: "Kernel parameters applied to ALL boot entries, including recovery mode. Use for critical settings that must always be active."
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
            }
        }

        // ── Tab 2: Kernel Entries ──
        ColumnLayout {
            spacing: 0

            ListView {
                id: kernelList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: kcm.kernelEntries
                clip: true

                delegate: QQC2.ItemDelegate {
                    width: kernelList.width

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            QQC2.Label {
                                text: modelData.title || modelData.version || "Unknown"
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            QQC2.Label {
                                text: modelData.linux || ""
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            QQC2.Label {
                                text: modelData.options || ""
                                font.family: "monospace"
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                color: Kirigami.Theme.disabledTextColor
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                                visible: text.length > 0
                            }
                        }

                        QQC2.Button {
                            text: "Set as Default"
                            icon.name: "favorite"
                            onClicked: kcm.setDefaultKernel(modelData.version)

                            QQC2.ToolTip.text: "Set this kernel as the default boot entry. The bootloader will automatically select it on next boot."
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.delay: 500
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: kernelList.count === 0
                    text: "No kernel entries found"
                    explanation: "BLS entries in /boot/loader/entries/ could not be read"
                }
            }
        }
    }
}
