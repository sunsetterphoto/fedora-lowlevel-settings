// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    id: root

    readonly property var categoryValues: ["", "kernel", "vm", "net", "fs", "dev"]

    header: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.SearchField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "Filter parameters..."
            onTextChanged: kcm.sysctlModel.searchText = text
        }

        QQC2.ComboBox {
            id: categoryCombo
            model: ["All", "kernel", "vm", "net", "fs", "dev"]
            onCurrentIndexChanged: kcm.sysctlModel.categoryFilter = root.categoryValues[currentIndex]
        }

        QQC2.CheckBox {
            id: modifiedCheck
            text: "Only modified"
            checked: kcm.sysctlModel.onlyModified
            onCheckedChanged: kcm.sysctlModel.onlyModified = checked
        }
    }

    ListView {
        id: paramList
        model: kcm.sysctlModel
        clip: true

        delegate: QQC2.ItemDelegate {
            width: paramList.width
            highlighted: model.isModified

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                // Key name (monospace, ~40% width)
                QQC2.Label {
                    text: model.key
                    font.family: "monospace"
                    elide: Text.ElideRight
                    Layout.preferredWidth: paramList.width * 0.4
                }

                // Editable value field (~30% width)
                QQC2.TextField {
                    id: valueField
                    text: model.isModified ? model.customValue : model.currentValue
                    Layout.preferredWidth: paramList.width * 0.3
                    Layout.fillWidth: true
                    onEditingFinished: {
                        if (text !== model.currentValue || model.isModified) {
                            kcm.sysctlModel.setValue(model.key, text)
                            kcm.needsSave = true
                        }
                    }
                }

                // Category badge
                QQC2.Label {
                    text: model.category
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    padding: Kirigami.Units.smallSpacing
                    background: Rectangle {
                        radius: 3
                        color: {
                            switch (model.category) {
                            case "kernel": return Qt.rgba(0.20, 0.60, 0.86, 0.2);
                            case "vm":     return Qt.rgba(0.61, 0.35, 0.71, 0.2);
                            case "net":    return Qt.rgba(0.15, 0.68, 0.38, 0.2);
                            case "fs":     return Qt.rgba(0.90, 0.49, 0.13, 0.2);
                            case "dev":    return Qt.rgba(0.58, 0.65, 0.65, 0.2);
                            default:       return Qt.rgba(0.50, 0.50, 0.50, 0.2);
                            }
                        }
                    }
                    Layout.alignment: Qt.AlignVCenter
                }

                // Reset button (only visible when modified)
                QQC2.ToolButton {
                    icon.name: "edit-clear"
                    visible: model.isModified
                    onClicked: {
                        kcm.sysctlModel.resetValue(model.key)
                        kcm.needsSave = true
                    }
                    QQC2.ToolTip.text: "Reset to default"
                    QQC2.ToolTip.visible: hovered
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: paramList.count === 0
            text: "No parameters found"
            explanation: "Try adjusting the category filter or clearing the search"
        }
    }
}
