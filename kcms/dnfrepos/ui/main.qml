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
            placeholderText: "Filter repositories..."
            onTextChanged: kcm.searchText = text
        }

        QQC2.Button {
            icon.name: "list-add"
            text: "Add COPR"
            onClicked: coprSheet.open()
        }

        QQC2.Button {
            icon.name: "view-refresh"
            text: "Refresh Cache"
            onClicked: kcm.refreshCache()
        }
    }

    view: ListView {
        id: repoList
        model: kcm.repos
        clip: true

        delegate: QQC2.ItemDelegate {
            id: repoDelegate
            width: repoList.width

            required property var modelData
            required property int index

            property bool expanded: false

            onClicked: expanded = !expanded

            contentItem: ColumnLayout {
                spacing: 0

                RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true

                        RowLayout {
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                source: repoDelegate.expanded
                                        ? "arrow-down" : "arrow-right"
                                implicitWidth: Kirigami.Units.iconSizes.small
                                implicitHeight: Kirigami.Units.iconSizes.small
                            }

                            QQC2.Label {
                                text: repoDelegate.modelData.name || repoDelegate.modelData.id
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // COPR badge
                            QQC2.Label {
                                visible: repoDelegate.modelData.isCopr
                                text: "COPR"
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                font.bold: true
                                color: "white"
                                padding: Kirigami.Units.smallSpacing
                                background: Rectangle {
                                    radius: 3
                                    color: "#2980b9"
                                }
                                Layout.alignment: Qt.AlignVCenter
                            }
                        }

                        QQC2.Label {
                            text: repoDelegate.modelData.baseurl || repoDelegate.modelData.metalink || ""
                            opacity: 0.6
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.leftMargin: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing
                        }
                    }

                    QQC2.Switch {
                        checked: repoDelegate.modelData.enabled
                        onToggled: {
                            kcm.toggleRepo(
                                repoDelegate.modelData.filename,
                                repoDelegate.modelData.id,
                                checked
                            )
                        }
                    }
                }

                // Expandable details
                ColumnLayout {
                    visible: repoDelegate.expanded
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing + Kirigami.Units.largeSpacing
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    spacing: 2

                    QQC2.Label {
                        text: "ID: " + repoDelegate.modelData.id
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }

                    QQC2.Label {
                        text: "GPG Check: " + (repoDelegate.modelData.gpgcheck ? "Yes" : "No")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }

                    QQC2.Label {
                        visible: (repoDelegate.modelData.gpgkey || "") !== ""
                        text: "GPG Key: " + (repoDelegate.modelData.gpgkey || "")
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    QQC2.Label {
                        text: "File: " + repoDelegate.modelData.filename
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        opacity: 0.7
                    }

                    // Remove COPR button
                    QQC2.Button {
                        visible: repoDelegate.modelData.isCopr
                        text: "Remove COPR"
                        icon.name: "list-remove"
                        onClicked: {
                            // Extract owner/project from repo id
                            // COPR repo ids typically look like: copr:copr.fedorainfracloud.org:owner:project
                            let parts = repoDelegate.modelData.id.split(":")
                            let project = ""
                            if (parts.length >= 4) {
                                project = parts[2] + "/" + parts[3]
                            } else {
                                project = repoDelegate.modelData.id
                            }
                            kcm.removeCoprRepo(project)
                        }
                    }
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: repoList.count === 0
            text: "No repositories found"
            explanation: searchField.text !== ""
                         ? "Try clearing the search filter"
                         : "No .repo files found in /etc/yum.repos.d/"
        }
    }

    // COPR add overlay sheet
    Kirigami.OverlaySheet {
        id: coprSheet

        title: "Add COPR Repository"

        ColumnLayout {
            implicitWidth: Kirigami.Units.gridUnit * 25
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                text: "Enter the COPR project in the format owner/project:"
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            QQC2.TextField {
                id: coprField
                Layout.fillWidth: true
                placeholderText: "owner/project"
                onAccepted: {
                    if (text.trim() !== "") {
                        kcm.addCoprRepo(text.trim())
                        coprSheet.close()
                        text = ""
                    }
                }
            }

            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.alignment: Qt.AlignRight

                QQC2.Button {
                    text: "Cancel"
                    icon.name: "dialog-cancel"
                    onClicked: {
                        coprSheet.close()
                        coprField.text = ""
                    }
                }

                QQC2.Button {
                    text: "Add"
                    icon.name: "list-add"
                    highlighted: true
                    enabled: coprField.text.trim() !== ""
                    onClicked: {
                        kcm.addCoprRepo(coprField.text.trim())
                        coprSheet.close()
                        coprField.text = ""
                    }
                }
            }
        }
    }
}
