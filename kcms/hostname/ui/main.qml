// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2026 Samuel

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCMUtils

KCMUtils.SimpleKCM {
    id: root

    readonly property var dnssecOptions: ["no", "allow-downgrade", "yes"]
    readonly property var dnsOverTlsOptions: ["no", "opportunistic", "yes"]

    Kirigami.FormLayout {
        id: form

        // ── Section 1: Hostname ──

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Hostname"
        }

        QQC2.TextField {
            Kirigami.FormData.label: "Pretty Hostname:"
            text: kcm.prettyHostname
            onTextEdited: kcm.prettyHostname = text
        }

        QQC2.TextField {
            Kirigami.FormData.label: "Static Hostname:"
            text: kcm.staticHostname
            onTextEdited: kcm.staticHostname = text
        }

        QQC2.TextField {
            Kirigami.FormData.label: "Icon Name:"
            text: kcm.iconName
            onTextEdited: kcm.iconName = text
        }

        QQC2.Label {
            Kirigami.FormData.label: "Chassis:"
            text: kcm.chassisType || "unknown"
        }

        QQC2.Label {
            Kirigami.FormData.label: "Machine ID:"
            text: kcm.machineId
            font.family: "monospace"
            textFormat: Text.PlainText

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    parent.selectAll()
                }
            }
        }

        // ── Section 2: Hosts ──

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "Hosts (/etc/hosts)"
        }

        QQC2.ScrollView {
            Kirigami.FormData.label: ""
            Layout.fillWidth: true
            implicitHeight: Kirigami.Units.gridUnit * 12

            QQC2.TextArea {
                id: hostsArea
                text: kcm.hostsContent
                font.family: "monospace"
                wrapMode: TextEdit.NoWrap
                onTextChanged: {
                    if (text !== kcm.hostsContent) {
                        kcm.hostsContent = text
                    }
                }
            }
        }

        // ── Section 3: DNS ──

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: "DNS (systemd-resolved)"
        }

        QQC2.TextField {
            Kirigami.FormData.label: "DNS Servers:"
            text: kcm.dnsServers
            placeholderText: "e.g. 1.1.1.1 8.8.8.8"
            onTextEdited: kcm.dnsServers = text
        }

        QQC2.TextField {
            Kirigami.FormData.label: "Fallback DNS:"
            text: kcm.fallbackDns
            placeholderText: "e.g. 9.9.9.9 149.112.112.112"
            onTextEdited: kcm.fallbackDns = text
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: "DNSSEC:"
            model: root.dnssecOptions
            currentIndex: {
                let idx = root.dnssecOptions.indexOf(kcm.dnssec)
                return idx >= 0 ? idx : 0
            }
            onActivated: (index) => {
                kcm.dnssec = root.dnssecOptions[index]
            }
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: "DNS over TLS:"
            model: root.dnsOverTlsOptions
            currentIndex: {
                let idx = root.dnsOverTlsOptions.indexOf(kcm.dnsOverTls)
                return idx >= 0 ? idx : 0
            }
            onActivated: (index) => {
                kcm.dnsOverTls = root.dnsOverTlsOptions[index]
            }
        }
    }
}
