// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.ScrollViewKCM {
    id: root

    header: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.SearchField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "Filter modules..."
            onTextChanged: kcm.searchText = text
        }

        QQC2.Button {
            icon.name: "view-refresh"
            text: "Refresh"
            onClicked: kcm.refresh()
        }
    }

    view: ListView {
        id: moduleList
        model: kcm.modules
        clip: true

        delegate: QQC2.ItemDelegate {
            id: moduleDelegate
            width: moduleList.width

            required property var modelData
            required property int index

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                // Module name and details
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true

                    RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            text: moduleDelegate.modelData.name
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // Blacklisted badge
                        QQC2.Label {
                            visible: moduleDelegate.modelData.isBlacklisted
                            text: "BLACKLISTED"
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            font.bold: true
                            color: "white"
                            padding: Kirigami.Units.smallSpacing
                            background: Rectangle {
                                radius: 3
                                color: "#c0392b"
                            }
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        QQC2.Label {
                            text: "Size: " + formatSize(moduleDelegate.modelData.size)
                            opacity: 0.6
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                        }

                        QQC2.Label {
                            text: "Used by: " + moduleDelegate.modelData.usedCount
                            opacity: 0.6
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                        }

                        QQC2.Label {
                            visible: moduleDelegate.modelData.usedBy !== undefined
                                     && moduleDelegate.modelData.usedBy !== ""
                            text: "(" + (moduleDelegate.modelData.usedBy || "") + ")"
                            opacity: 0.6
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                // Action buttons
                RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    // Info button
                    QQC2.ToolButton {
                        icon.name: "view-visible"
                        onClicked: {
                            kcm.getModuleInfo(moduleDelegate.modelData.name)
                            infoSheet.moduleName = moduleDelegate.modelData.name
                            infoSheet.open()
                        }
                        QQC2.ToolTip.text: "Module Info"
                        QQC2.ToolTip.visible: hovered
                    }

                    // Blacklist / Unblacklist toggle
                    QQC2.ToolButton {
                        icon.name: moduleDelegate.modelData.isBlacklisted
                                   ? "edit-undo" : "process-stop"
                        onClicked: {
                            if (moduleDelegate.modelData.isBlacklisted) {
                                kcm.unblacklistModule(moduleDelegate.modelData.name)
                            } else {
                                kcm.blacklistModule(moduleDelegate.modelData.name)
                            }
                        }
                        QQC2.ToolTip.text: moduleDelegate.modelData.isBlacklisted
                                           ? "Remove from blacklist" : "Add to blacklist"
                        QQC2.ToolTip.visible: hovered
                    }

                    // Unload button (only if usedCount == 0)
                    QQC2.ToolButton {
                        icon.name: "list-remove"
                        visible: moduleDelegate.modelData.usedCount === 0
                        onClicked: kcm.unloadModule(moduleDelegate.modelData.name)
                        QQC2.ToolTip.text: "Unload module"
                        QQC2.ToolTip.visible: hovered
                    }
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: moduleList.count === 0
            text: "No modules found"
            explanation: "Try clearing the search filter"
        }
    }

    // Detail overlay sheet for module info
    Kirigami.OverlaySheet {
        id: infoSheet

        property string moduleName: ""

        title: "Module: " + moduleName

        QQC2.ScrollView {
            implicitWidth: Kirigami.Units.gridUnit * 30
            implicitHeight: Kirigami.Units.gridUnit * 20

            QQC2.TextArea {
                text: kcm.selectedModuleInfo
                readOnly: true
                font.family: "monospace"
                wrapMode: TextEdit.Wrap
            }
        }
    }

    function formatSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + " KiB"
        return (bytes / (1024 * 1024)).toFixed(1) + " MiB"
    }
}
