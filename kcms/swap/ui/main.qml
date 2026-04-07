// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    id: root

    function bytesValue(formatted) {
        // Parse "X.Y GiB" style strings back to bytes for progress bars
        if (!formatted || formatted === "N/A") return 0
        var parts = formatted.split(" ")
        if (parts.length < 2) return 0
        var val = parseFloat(parts[0])
        var unit = parts[1]
        switch (unit) {
        case "B":   return val
        case "KiB": return val * 1024
        case "MiB": return val * 1024 * 1024
        case "GiB": return val * 1024 * 1024 * 1024
        case "TiB": return val * 1024 * 1024 * 1024 * 1024
        default:    return 0
        }
    }

    Kirigami.FormLayout {
        id: form

        // ── Section: zram Configuration ──

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "zram Configuration"
        }

        QQC2.TextField {
            Kirigami.FormData.label: "Size:"
            text: kcm.zramSize
            placeholderText: "min(ram, 8192)"
            onTextEdited: kcm.zramSize = text
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: "Algorithm:"
            model: kcm.availableAlgorithms
            currentIndex: {
                var idx = kcm.availableAlgorithms.indexOf(kcm.zramAlgorithm)
                return idx >= 0 ? idx : 0
            }
            onActivated: (index) => {
                kcm.zramAlgorithm = kcm.availableAlgorithms[index]
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            text: "Changes require a reboot to take effect"
            visible: true
        }

        // ── Section: Current zram Status ──

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Current zram Status"
        }

        QQC2.Label {
            Kirigami.FormData.label: "Disk Size:"
            text: kcm.zramCurrentSize
        }

        QQC2.Label {
            Kirigami.FormData.label: "Original Data:"
            text: kcm.zramOrigDataSize
        }

        QQC2.Label {
            Kirigami.FormData.label: "Compressed Data:"
            text: kcm.zramComprDataSize
        }

        QQC2.Label {
            Kirigami.FormData.label: "Compression Ratio:"
            text: {
                var orig = root.bytesValue(kcm.zramOrigDataSize)
                var compr = root.bytesValue(kcm.zramComprDataSize)
                if (compr > 0 && orig > 0) {
                    return (orig / compr).toFixed(2) + ":1"
                }
                return "N/A"
            }
        }

        QQC2.ProgressBar {
            Kirigami.FormData.label: "zram Usage:"
            Layout.fillWidth: true
            from: 0
            to: root.bytesValue(kcm.zramCurrentSize)
            value: root.bytesValue(kcm.zramOrigDataSize)

            QQC2.ToolTip.text: kcm.zramOrigDataSize + " / " + kcm.zramCurrentSize
            QQC2.ToolTip.visible: hovered
        }

        // ── Section: Swap Areas ──

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Swap Areas"
        }

        Repeater {
            model: kcm.swapEntries

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                required property var modelData
                required property int index

                Kirigami.FormData.label: index === 0 ? "" : ""

                RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Layout.fillWidth: true

                    QQC2.Label {
                        text: modelData.filename
                        font.family: "monospace"
                        font.bold: true
                    }

                    QQC2.Label {
                        text: modelData.type
                        opacity: 0.7
                    }

                    QQC2.Label {
                        text: modelData.sizeFormatted
                    }

                    QQC2.Label {
                        text: "Used: " + modelData.usedFormatted
                        opacity: 0.7
                    }

                    QQC2.Label {
                        text: "Priority: " + modelData.priority
                        opacity: 0.7
                    }

                    Item { Layout.fillWidth: true }
                }

                QQC2.ProgressBar {
                    Layout.fillWidth: true
                    from: 0
                    to: modelData.size
                    value: modelData.used

                    QQC2.ToolTip.text: modelData.usedFormatted + " / " + modelData.sizeFormatted
                    QQC2.ToolTip.visible: hovered
                }
            }
        }

        Kirigami.PlaceholderMessage {
            visible: kcm.swapEntries.length === 0
            text: "No swap areas found"
            explanation: "No swap partitions or files are currently active"
            Layout.fillWidth: true
        }

        // ── Section: Memory Info ──

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Memory Info"
        }

        QQC2.Label {
            Kirigami.FormData.label: "Total RAM:"
            text: kcm.totalRam
        }

        QQC2.Label {
            Kirigami.FormData.label: "vm.swappiness:"
            text: kcm.swappiness
        }
    }
}
