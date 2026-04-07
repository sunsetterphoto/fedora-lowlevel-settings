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
                text: "Sudoers"
                icon.name: "document-edit"
            }
            QQC2.TabButton {
                text: "SELinux"
                icon.name: "security-high"
            }
            QQC2.TabButton {
                text: "Polkit"
                icon.name: "system-lock-screen"
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

        // ── Tab 1: Sudoers ──
        ColumnLayout {
            spacing: 0

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                type: Kirigami.MessageType.Information
                text: "Changes are validated with visudo before writing"
                visible: true
            }

            QQC2.Label {
                text: "Rules define which users can run which commands with elevated privileges. Each file in /etc/sudoers.d/ contains one or more rules. Syntax: 'username ALL=(ALL) COMMAND' or 'username ALL=(ALL) NOPASSWD: COMMAND'."
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
            }

            ListView {
                id: sudoersList
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: kcm.sudoersFiles
                clip: true

                delegate: ColumnLayout {
                    id: sudoersDelegate
                    width: sudoersList.width

                    required property var modelData
                    required property int index

                    property bool expanded: false

                    QQC2.ItemDelegate {
                        Layout.fillWidth: true
                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                source: sudoersDelegate.expanded
                                        ? "arrow-down" : "arrow-right"
                                implicitWidth: Kirigami.Units.iconSizes.small
                                implicitHeight: Kirigami.Units.iconSizes.small
                            }

                            Kirigami.Icon {
                                source: "document-text"
                                implicitWidth: Kirigami.Units.iconSizes.small
                                implicitHeight: Kirigami.Units.iconSizes.small
                            }

                            QQC2.Label {
                                text: sudoersDelegate.modelData.filename
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            QQC2.ToolButton {
                                icon.name: "document-edit"
                                onClicked: {
                                    editSheet.editFilename = sudoersDelegate.modelData.filename
                                    editSheet.editContent = sudoersDelegate.modelData.content
                                    editSheet.open()
                                }
                                QQC2.ToolTip.text: "Edit"
                                QQC2.ToolTip.visible: hovered
                            }
                        }
                        onClicked: sudoersDelegate.expanded = !sudoersDelegate.expanded
                    }

                    QQC2.TextArea {
                        visible: sudoersDelegate.expanded
                        Layout.fillWidth: true
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 8
                        Layout.leftMargin: Kirigami.Units.largeSpacing * 2
                        Layout.rightMargin: Kirigami.Units.largeSpacing
                        text: sudoersDelegate.modelData.content
                        readOnly: true
                        font.family: "monospace"
                        wrapMode: TextEdit.Wrap
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: sudoersList.count === 0
                    text: "No sudoers files found"
                    explanation: "No files in /etc/sudoers.d/"
                }
            }
        }

        // ── Tab 2: SELinux ──
        ColumnLayout {
            spacing: 0

            // Mode selector
            Kirigami.FormLayout {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing

                QQC2.ComboBox {
                    Kirigami.FormData.label: "SELinux Mode:"
                    id: selinuxModeCombo
                    model: ["Enforcing", "Permissive"]
                    currentIndex: kcm.selinuxMode === "Permissive" ? 1 : 0
                    enabled: kcm.selinuxMode !== "Disabled" && kcm.selinuxMode !== "Unknown"
                    onActivated: {
                        kcm.setSelinuxMode(currentText)
                    }
                }

                QQC2.Label {
                    text: selinuxModeCombo.currentIndex === 0
                          ? "Enforcing: SELinux policies are enforced. Violations are blocked and logged."
                          : "Permissive: SELinux policies are not enforced but violations are logged. Useful for debugging."
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                    visible: kcm.selinuxMode !== "Disabled" && kcm.selinuxMode !== "Unknown"
                }

                QQC2.Label {
                    text: "Switching between modes is immediate. A reboot is NOT required."
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                    visible: kcm.selinuxMode !== "Disabled" && kcm.selinuxMode !== "Unknown"
                }

                QQC2.Label {
                    visible: kcm.selinuxMode === "Disabled"
                    text: "SELinux is disabled. Enable it in /etc/selinux/config and reboot."
                    opacity: 0.7
                }

                QQC2.Label {
                    visible: kcm.selinuxMode === "Unknown"
                    text: "SELinux status could not be determined (getenforce not found)."
                    opacity: 0.7
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            // Search field for booleans
            Kirigami.SearchField {
                id: boolSearchField
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                placeholderText: "Filter SELinux booleans..."
            }

            ListView {
                id: boolList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                model: {
                    let filter = boolSearchField.text.toLowerCase()
                    if (filter === "") {
                        return kcm.selinuxBooleans
                    }
                    return kcm.selinuxBooleans.filter(function(b) {
                        return b.name.toLowerCase().indexOf(filter) !== -1
                    })
                }

                delegate: QQC2.ItemDelegate {
                    id: boolDelegate
                    width: boolList.width

                    required property var modelData
                    required property int index

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: boolDelegate.modelData.name
                            font.family: "monospace"
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        QQC2.Switch {
                            checked: boolDelegate.modelData.value
                            onToggled: {
                                kcm.setSelinuxBool(boolDelegate.modelData.name, checked)
                            }
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: boolList.count === 0
                    text: boolSearchField.text !== ""
                          ? "No matching booleans"
                          : "No SELinux booleans found"
                    explanation: boolSearchField.text !== ""
                                 ? "Try a different search term"
                                 : "SELinux may be disabled or getenforce is not available"
                }
            }
        }

        // ── Tab 3: Polkit ──
        ColumnLayout {
            spacing: 0

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                type: Kirigami.MessageType.Information
                text: "Polkit actions from /usr/share/polkit-1/actions/ (read-only)"
                visible: true
            }

            QQC2.Label {
                text: "Polkit rules control which users can perform privileged actions without entering a password. Rules are JavaScript files evaluated in order."
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
            }

            // Search field for polkit actions
            Kirigami.SearchField {
                id: polkitSearchField
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                placeholderText: "Filter polkit actions..."
            }

            ListView {
                id: polkitList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                model: {
                    let filter = polkitSearchField.text.toLowerCase()
                    if (filter === "") {
                        return kcm.polkitActions
                    }
                    return kcm.polkitActions.filter(function(a) {
                        return a.id.toLowerCase().indexOf(filter) !== -1
                               || (a.description || "").toLowerCase().indexOf(filter) !== -1
                    })
                }

                delegate: QQC2.ItemDelegate {
                    id: polkitDelegate
                    width: polkitList.width

                    required property var modelData
                    required property int index

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        ColumnLayout {
                            spacing: 0
                            Layout.fillWidth: true

                            QQC2.Label {
                                text: polkitDelegate.modelData.id
                                font.family: "monospace"
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            QQC2.Label {
                                text: polkitDelegate.modelData.description || ""
                                visible: (polkitDelegate.modelData.description || "") !== ""
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }

                        // allowActive badge
                        QQC2.Label {
                            visible: (polkitDelegate.modelData.allowActive || "") !== ""
                            text: polkitDelegate.modelData.allowActive || ""
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            padding: Kirigami.Units.smallSpacing
                            background: Rectangle {
                                radius: 3
                                color: {
                                    let v = polkitDelegate.modelData.allowActive || "";
                                    if (v === "yes") return Qt.rgba(0.15, 0.68, 0.38, 0.2);
                                    if (v === "no") return Qt.rgba(0.75, 0.22, 0.17, 0.2);
                                    if (v === "auth_admin" || v === "auth_admin_keep")
                                        return Qt.rgba(0.90, 0.49, 0.13, 0.2);
                                    if (v === "auth_self" || v === "auth_self_keep")
                                        return Qt.rgba(0.20, 0.60, 0.86, 0.2);
                                    return Qt.rgba(0.58, 0.65, 0.65, 0.2);
                                }
                            }
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: polkitList.count === 0
                    text: polkitSearchField.text !== ""
                          ? "No matching polkit actions"
                          : "No polkit actions found"
                }
            }
        }
    }

    // ── Sudoers edit overlay sheet ──
    Kirigami.OverlaySheet {
        id: editSheet

        property string editFilename: ""
        property string editContent: ""

        title: "Edit: " + editFilename

        ColumnLayout {
            implicitWidth: Kirigami.Units.gridUnit * 35
            spacing: Kirigami.Units.largeSpacing

            QQC2.TextArea {
                id: editTextArea
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 18
                font.family: "monospace"
                wrapMode: TextEdit.Wrap
                text: editSheet.editContent
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
                    text: "Save"
                    icon.name: "document-save"
                    highlighted: true
                    onClicked: {
                        kcm.saveSudoersFile(editSheet.editFilename, editTextArea.text)
                        editSheet.close()
                    }
                }
            }
        }
    }
}
