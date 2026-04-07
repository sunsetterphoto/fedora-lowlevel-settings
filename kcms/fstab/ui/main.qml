// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.ScrollViewKCM {
    id: root

    property int editIndex: -1
    property var editEntry: ({})

    headerPaddingEnabled: false

    header: ColumnLayout {
        spacing: 0

        Kirigami.InlineMessage {
            id: rootWarning
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            text: "You are editing the root filesystem entry. Only mount options can be changed."
            visible: false
        }
    }

    view: ListView {
        id: entryList
        model: kcm.fstabEntries
        clip: true

        delegate: QQC2.ItemDelegate {
            id: entryDelegate
            width: entryList.width

            required property var modelData
            required property int index

            visible: !modelData.isComment
            height: modelData.isComment ? 0 : implicitHeight

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                // Mounted status dot
                Rectangle {
                    width: Kirigami.Units.iconSizes.small
                    height: Kirigami.Units.iconSizes.small
                    radius: width / 2
                    color: entryDelegate.modelData.isMounted ? "#27ae60" : "#95a5a6"

                    QQC2.ToolTip.text: entryDelegate.modelData.isMounted ? "Mounted" : "Not mounted"
                    QQC2.ToolTip.visible: statusMa.containsMouse

                    MouseArea {
                        id: statusMa
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

                // Lock icon for root fs
                Kirigami.Icon {
                    source: "object-locked"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                    visible: entryDelegate.modelData.isRootFs

                    QQC2.ToolTip.text: "Root filesystem (protected)"
                    QQC2.ToolTip.visible: lockMa.containsMouse

                    MouseArea {
                        id: lockMa
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }

                // Device
                QQC2.Label {
                    text: entryDelegate.modelData.device || ""
                    font.family: "monospace"
                    elide: Text.ElideMiddle
                    Layout.preferredWidth: entryList.width * 0.25
                }

                // Mountpoint
                QQC2.Label {
                    text: entryDelegate.modelData.mountpoint || ""
                    font.family: "monospace"
                    elide: Text.ElideRight
                    Layout.preferredWidth: entryList.width * 0.15
                }

                // Filesystem type
                QQC2.Label {
                    text: entryDelegate.modelData.fstype || ""
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    padding: Kirigami.Units.smallSpacing
                    background: Rectangle {
                        radius: 3
                        color: Kirigami.Theme.highlightColor
                        opacity: 0.15
                    }
                }

                // Options
                QQC2.Label {
                    text: entryDelegate.modelData.options || ""
                    font.family: "monospace"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                // Edit button
                QQC2.ToolButton {
                    icon.name: "document-edit"
                    onClicked: {
                        root.editIndex = entryDelegate.index
                        root.editEntry = {
                            device: entryDelegate.modelData.device || "",
                            mountpoint: entryDelegate.modelData.mountpoint || "",
                            fstype: entryDelegate.modelData.fstype || "",
                            options: entryDelegate.modelData.options || "",
                            dump: entryDelegate.modelData.dump || 0,
                            pass: entryDelegate.modelData.pass || 0,
                            isRootFs: entryDelegate.modelData.isRootFs || false
                        }

                        editDeviceField.text = root.editEntry.device
                        editMountpointField.text = root.editEntry.mountpoint
                        editTypeCombo.currentIndex = editTypeCombo.indexOfValue(root.editEntry.fstype)
                        if (editTypeCombo.currentIndex < 0) editTypeCombo.currentIndex = 0
                        editOptionsField.text = root.editEntry.options
                        editDumpSpin.value = root.editEntry.dump
                        editPassSpin.value = root.editEntry.pass

                        editDeviceField.enabled = !root.editEntry.isRootFs
                        editMountpointField.enabled = !root.editEntry.isRootFs
                        rootWarning.visible = root.editEntry.isRootFs

                        editSheet.open()
                    }

                    QQC2.ToolTip.text: "Edit entry"
                    QQC2.ToolTip.visible: hovered
                }

                // Delete button (hidden for root fs)
                QQC2.ToolButton {
                    icon.name: "edit-delete"
                    visible: !entryDelegate.modelData.isRootFs
                    onClicked: {
                        kcm.removeEntry(entryDelegate.index)
                    }

                    QQC2.ToolTip.text: "Remove entry"
                    QQC2.ToolTip.visible: hovered
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: entryList.count === 0
            text: "No fstab entries found"
            explanation: "Could not read /etc/fstab"
        }
    }

    footer: RowLayout {
        QQC2.Button {
            text: "Add Entry"
            icon.name: "list-add"
            onClicked: {
                root.editIndex = -1
                root.editEntry = {}

                editDeviceField.text = ""
                editMountpointField.text = ""
                editTypeCombo.currentIndex = 0
                editOptionsField.text = "defaults"
                editDumpSpin.value = 0
                editPassSpin.value = 0

                editDeviceField.enabled = true
                editMountpointField.enabled = true
                rootWarning.visible = false

                editSheet.open()
            }
        }
    }

    // ── Edit / Add Overlay Sheet ──
    Kirigami.OverlaySheet {
        id: editSheet
        title: root.editIndex >= 0 ? "Edit Mount Entry" : "Add Mount Entry"

        Kirigami.FormLayout {
            implicitWidth: Kirigami.Units.gridUnit * 25

            QQC2.TextField {
                id: editDeviceField
                Kirigami.FormData.label: "Device:"
                placeholderText: "/dev/sdXN or UUID=..."
                Layout.fillWidth: true
            }

            QQC2.Label {
                text: "Block device path or UUID. Using UUID= is recommended as it survives disk reordering."
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
            }

            QQC2.TextField {
                id: editMountpointField
                Kirigami.FormData.label: "Mount Point:"
                placeholderText: "/mnt/data"
                Layout.fillWidth: true
            }

            QQC2.Label {
                text: "Directory where the filesystem will be accessible. Use 'none' for swap."
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
            }

            QQC2.ComboBox {
                id: editTypeCombo
                Kirigami.FormData.label: "Filesystem Type:"
                model: ["ext4", "btrfs", "xfs", "vfat", "ntfs", "swap", "nfs", "tmpfs"]
                Layout.fillWidth: true
            }

            QQC2.Label {
                text: kcm.fsTypeDescription(editTypeCombo.currentText)
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
                visible: text.length > 0
            }

            QQC2.TextField {
                id: editOptionsField
                Kirigami.FormData.label: "Options:"
                placeholderText: "defaults"
                Layout.fillWidth: true
            }

            QQC2.Label {
                text: "Common options: defaults, noatime (don't update access times, better performance), discard (enable TRIM for SSDs), compress=zstd (btrfs compression), nofail (don't fail boot if device missing), x-systemd.automount (mount on first access)"
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
            }

            QQC2.SpinBox {
                id: editDumpSpin
                Kirigami.FormData.label: "Dump:"
                from: 0
                to: 1
            }

            QQC2.Label {
                text: "0 = don't backup with dump (default), 1 = include in dump backups (rarely used today)"
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
            }

            QQC2.SpinBox {
                id: editPassSpin
                Kirigami.FormData.label: "Pass:"
                from: 0
                to: 2
            }

            QQC2.Label {
                text: "0 = don't check with fsck, 1 = check first (root filesystem), 2 = check after root (other partitions)"
                wrapMode: Text.WordWrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                opacity: 0.7
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.topMargin: Kirigami.Units.largeSpacing

                QQC2.Button {
                    text: root.editIndex >= 0 ? "Save" : "Add"
                    icon.name: root.editIndex >= 0 ? "document-save" : "list-add"
                    enabled: editDeviceField.text.length > 0
                             && (editMountpointField.text.length > 0
                                 || editTypeCombo.currentText === "swap")

                    onClicked: {
                        var entry = {
                            device: editDeviceField.text,
                            mountpoint: editMountpointField.text,
                            fstype: editTypeCombo.currentText,
                            options: editOptionsField.text || "defaults",
                            dump: editDumpSpin.value,
                            pass: editPassSpin.value
                        }

                        if (root.editIndex >= 0) {
                            kcm.updateEntry(root.editIndex, entry)
                        } else {
                            kcm.addEntry(entry)
                        }

                        editSheet.close()
                    }
                }

                QQC2.Button {
                    text: "Cancel"
                    onClicked: editSheet.close()
                }
            }
        }
    }
}
