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
            text: "System"
            icon.name: "computer"
        }
        QQC2.TabButton {
            text: "User Session"
            icon.name: "user-identity"
        }
        QQC2.TabButton {
            text: "Profile Scripts"
            icon.name: "text-x-script"
        }
    }

    StackLayout {
        currentIndex: tabBar.currentIndex
        anchors.fill: parent

        // ── Tab 1: System Variables ──
        ColumnLayout {
            spacing: 0

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                type: Kirigami.MessageType.Information
                text: "System-wide variables in /etc/environment apply to all users and sessions. Changes require re-login."
                visible: true
            }

            ListView {
                id: systemList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: kcm.systemVars
                clip: true

                delegate: QQC2.ItemDelegate {
                    id: sysDelegate
                    width: systemList.width

                    required property var modelData
                    required property int index

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: sysDelegate.modelData.key
                            font.bold: true
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                            elide: Text.ElideRight
                        }

                        QQC2.Label {
                            text: "="
                            opacity: 0.5
                        }

                        QQC2.Label {
                            text: sysDelegate.modelData.value
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            font.family: "monospace"
                        }

                        QQC2.ToolButton {
                            icon.name: "document-edit"
                            onClicked: {
                                editSheet.editScope = "system"
                                editSheet.editKey = sysDelegate.modelData.key
                                editSheet.editValue = sysDelegate.modelData.value
                                editSheet.isNew = false
                                editSheet.open()
                            }
                            QQC2.ToolTip.text: "Edit"
                            QQC2.ToolTip.visible: hovered
                        }

                        QQC2.ToolButton {
                            icon.name: "edit-delete"
                            onClicked: kcm.removeSystemVar(sysDelegate.modelData.key)
                            QQC2.ToolTip.text: "Delete"
                            QQC2.ToolTip.visible: hovered
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: systemList.count === 0
                    text: "No system environment variables"
                    explanation: "/etc/environment is empty or not readable"
                }
            }

            QQC2.Button {
                Layout.alignment: Qt.AlignLeft
                Layout.margins: Kirigami.Units.largeSpacing
                text: "Add Variable"
                icon.name: "list-add"
                onClicked: {
                    editSheet.editScope = "system"
                    editSheet.editKey = ""
                    editSheet.editValue = ""
                    editSheet.isNew = true
                    editSheet.open()
                }
            }
        }

        // ── Tab 2: User Session Variables ──
        ColumnLayout {
            spacing: 0

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                type: Kirigami.MessageType.Information
                text: "User session variables from ~/.config/environment.d/ apply to your session only. Changes take effect on next login."
                visible: true
            }

            ListView {
                id: userList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: kcm.userVars
                clip: true

                delegate: QQC2.ItemDelegate {
                    id: userDelegate
                    width: userList.width

                    required property var modelData
                    required property int index

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: userDelegate.modelData.key
                            font.bold: true
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                            elide: Text.ElideRight
                        }

                        QQC2.Label {
                            text: "="
                            opacity: 0.5
                        }

                        QQC2.Label {
                            text: userDelegate.modelData.value
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            font.family: "monospace"
                        }

                        QQC2.ToolButton {
                            icon.name: "document-edit"
                            onClicked: {
                                editSheet.editScope = "user"
                                editSheet.editKey = userDelegate.modelData.key
                                editSheet.editValue = userDelegate.modelData.value
                                editSheet.isNew = false
                                editSheet.open()
                            }
                            QQC2.ToolTip.text: "Edit"
                            QQC2.ToolTip.visible: hovered
                        }

                        QQC2.ToolButton {
                            icon.name: "edit-delete"
                            onClicked: kcm.removeUserVar(userDelegate.modelData.key)
                            QQC2.ToolTip.text: "Delete"
                            QQC2.ToolTip.visible: hovered
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: userList.count === 0
                    text: "No user environment variables"
                    explanation: "No variables in ~/.config/environment.d/nsa-fsm.conf"
                }
            }

            QQC2.Button {
                Layout.alignment: Qt.AlignLeft
                Layout.margins: Kirigami.Units.largeSpacing
                text: "Add Variable"
                icon.name: "list-add"
                onClicked: {
                    editSheet.editScope = "user"
                    editSheet.editKey = ""
                    editSheet.editValue = ""
                    editSheet.isNew = true
                    editSheet.open()
                }
            }
        }

        // ── Tab 3: Profile Scripts (read-only) ──
        ColumnLayout {
            spacing: 0

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                type: Kirigami.MessageType.Information
                text: "Scripts from /etc/profile.d/ (read-only)"
                visible: true
            }

            ListView {
                id: profileList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: kcm.profileScripts
                clip: true

                delegate: QQC2.ItemDelegate {
                    id: profileDelegate
                    width: profileList.width

                    required property string modelData
                    required property int index

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "text-x-script"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                        }

                        QQC2.Label {
                            text: profileDelegate.modelData
                            font.family: "monospace"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: profileList.count === 0
                    text: "No profile scripts found"
                    explanation: "/etc/profile.d/ is empty or not readable"
                }
            }
        }
    }

    // ── Add/Edit overlay sheet ──
    Kirigami.OverlaySheet {
        id: editSheet

        property string editScope: "system"
        property string editKey: ""
        property string editValue: ""
        property bool isNew: true

        title: (isNew ? "Add" : "Edit") + " " +
               (editScope === "system" ? "System" : "User") + " Variable"

        ColumnLayout {
            implicitWidth: Kirigami.Units.gridUnit * 25
            spacing: Kirigami.Units.largeSpacing

            Kirigami.FormLayout {
                QQC2.TextField {
                    id: keyField
                    Kirigami.FormData.label: "Key:"
                    Layout.fillWidth: true
                    text: editSheet.editKey
                    readOnly: !editSheet.isNew
                    placeholderText: "VARIABLE_NAME"
                }

                QQC2.TextField {
                    id: valueField
                    Kirigami.FormData.label: "Value:"
                    Layout.fillWidth: true
                    text: editSheet.editValue
                    placeholderText: "value"
                    onAccepted: saveButton.clicked()
                }
            }

            QQC2.Label {
                text: "Common variables: PATH (search path for commands), EDITOR (default text editor), LANG (system language), XDG_DATA_DIRS (data file search paths)."
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.alignment: Qt.AlignRight

                QQC2.Button {
                    text: "Cancel"
                    icon.name: "dialog-cancel"
                    onClicked: editSheet.close()
                }

                QQC2.Button {
                    id: saveButton
                    text: editSheet.isNew ? "Add" : "Save"
                    icon.name: editSheet.isNew ? "list-add" : "document-save"
                    highlighted: true
                    enabled: keyField.text.trim() !== ""
                    onClicked: {
                        let key = keyField.text.trim()
                        let value = valueField.text

                        if (editSheet.editScope === "system") {
                            if (editSheet.isNew) {
                                kcm.addSystemVar(key, value)
                            } else {
                                kcm.setSystemVar(key, value)
                            }
                        } else {
                            if (editSheet.isNew) {
                                kcm.addUserVar(key, value)
                            } else {
                                kcm.setUserVar(key, value)
                            }
                        }
                        editSheet.close()
                    }
                }
            }
        }
    }
}
