# Fedora Core Setting Extension -- Design Specification

**Datum:** 2026-04-07
**Autor:** sunsetterphoto + Claude
**Status:** Draft
**Zielplattform:** Fedora 43, KDE Plasma 6.6.3, KF6 6.24.0, Qt 6.10.2

---

## 1. Zielsetzung

Ein modulares KDE System Settings Plugin-Paket, das alle systemnahen Linux-Konfigurationen abdeckt, die KDE Systemsettings aktuell nicht bietet. Erscheint als eigene Kategorie **"System-Administration"** in der Seitenleiste.

### Nicht-Ziele

- Kein Ersatz für bestehende KDE-KCMs (Netzwerk, Audio, Display, Users)
- Kein Ersatz für plasma-firewall
- Keine Distro-Abstraktion -- Fedora-first, andere Distros per Fork

---

## 2. Architektur

```
┌──────────────────────────────────────────────────────┐
│              KDE Systemsettings                       │
│  ┌────────────────────────────────────────────────┐   │
│  │  Kategorie: "System-Administration"            │   │
│  │  12 KCM-Module (je ein .so Plugin + QML UI)    │   │
│  └──────────────────┬─────────────────────────────┘   │
└─────────────────────┼─────────────────────────────────┘
                      │
              ┌───────▼────────┐
              │ libfcse-    │
              │ common.so      │
              │ (Shared Lib)   │
              └──┬──────────┬──┘
                 │          │
        ┌────────▼──┐  ┌───▼──────────────┐
        │  D-Bus    │  │  KAuth Helper    │
        │  direkt   │  │  fcse-helper  │
        │           │  │  (root-Prozess)  │
        └───────────┘  └──────────────────┘
```

### Drei Privilege-Modelle

| Modell | Wann | Beispiel |
|--------|------|----------|
| **D-Bus direkt** | Service hat eigene polkit-Integration | systemd, firewalld, UDisks2, hostname1 |
| **KAuth-Helper** | Direkter Dateizugriff auf /etc/ nötig | sysctl, GRUB, fstab, modprobe, sudoers |
| **Unprivilegiert** | Daten über User-Zugang lesbar | Journal (systemd-journal Gruppe) |

---

## 3. Module

### Modul 1: systemd Services

**KCM-Name:** `kcm_fcse_services`
**Privilege:** D-Bus direkt (`org.freedesktop.systemd1`)

**Features:**
- ListView aller Units mit Suchfeld und Typ-Filter (service, timer, socket, mount, target)
- Zwei Tabs: System-Services | User-Services (Session-Bus)
- Status-Badges: running (gruen), stopped (grau), failed (rot), masked (orange)
- Aktionen: Start / Stop / Restart / Enable / Disable / Mask
- Unit-File-Inhalt read-only anzeigen
- Log-Snippet pro Service (letzte 50 Zeilen aus Journal)
- `setInteractiveAuthorizationAllowed(true)` auf allen D-Bus-Calls

**D-Bus-Interface:**
- `org.freedesktop.systemd1.Manager`: `ListUnits()`, `ListUnitFiles()`, `StartUnit()`, `StopUnit()`, `RestartUnit()`, `EnableUnitFiles()`, `DisableUnitFiles()`, `MaskUnitFiles()`, `UnmaskUnitFiles()`, `GetUnitFileState()`, `Reload()`
- User-Services ueber Session-Bus statt System-Bus

**QML:** `ScrollViewKCM` mit ListView + Kirigami.SearchField

---

### Modul 2: sysctl Parameter

**KCM-Name:** `kcm_fcse_sysctl`
**Privilege:** KAuth-Helper (`org.kde.fcse.sysctl.write`)

**Features:**
- Kategorisierte Baumansicht: kernel.*, vm.*, net.*, fs.*, dev.*
- Pro Parameter: aktueller Wert, Default-Wert (aus /proc/sys), Beschreibung
- Inline-Editing mit Validierung (Typ-Check: Integer, String, Boolean)
- Nur geaenderte Werte werden nach `/etc/sysctl.d/99-fcse.conf` geschrieben
- Preset-System:
  - "Desktop Balanced" (Fedora defaults)
  - "Low Latency" (swappiness=10, CFS tweaks)
  - "Network Server" (Buffer-Groessen hoch, Connection-Tracking)
- Apply fuehrt `sysctl --system` via Helper aus

**Datenquellen:**
- Lesen: `/proc/sys/` (kein Root noetig)
- Aktuelle Custom-Werte: `/etc/sysctl.d/*.conf` parsen
- Schreiben: KAuth-Helper -> `/etc/sysctl.d/99-fcse.conf` + `sysctl --system`

**QML:** `ScrollViewKCM` mit TreeView + Kirigami.FormLayout fuer Details

---

### Modul 3: Bootloader

**KCM-Name:** `kcm_fcse_bootloader`
**Privilege:** KAuth-Helper (`org.kde.fcse.grub.write`, `org.kde.fcse.bls.write`)

**Features:**
- Bootloader-Erkennung: GRUB2 vs systemd-boot (prueft `/boot/efi/EFI/fedora/grub.cfg` und `/boot/efi/loader/loader.conf`)
- **GRUB-Tab:**
  - `/etc/default/grub` Key-Value-Editor
  - GRUB_CMDLINE_LINUX_DEFAULT als Tag-Editor (Parameter einzeln hinzufuegen/entfernen)
  - GRUB_TIMEOUT, GRUB_DEFAULT
  - Vorschau der generierten Config vor dem Anwenden
  - Apply: Backup -> Schreiben -> `grub2-mkconfig -o /boot/grub2/grub.cfg`
- **Kernel-Eintraege-Tab (BLS):**
  - Liste aller `/boot/loader/entries/*.conf`
  - Pro Eintrag: title, version, linux, initrd, options
  - Kernel-Optionen editieren (pro Eintrag oder global via `grubby --update-kernel`)
  - Default-Kernel setzen (`grubby --set-default`)
- BLS-Entries lesen benoetigt Root -> KAuth-Helper auch fuer Lese-Operationen

**QML:** `SimpleKCM` mit TabBar (GRUB | Kernel-Eintraege)

---

### Modul 4: fstab / Mounts

**KCM-Name:** `kcm_fcse_fstab`
**Privilege:** KAuth-Helper (`org.kde.fcse.fstab.write`) + D-Bus (`org.freedesktop.UDisks2`)

**Features:**
- Tabelle aller `/etc/fstab`-Eintraege: Device, Mountpoint, Typ, Optionen, Dump, Pass
- Status-Spalte: gemounted (gruen) / nicht gemounted (grau) / fehler (rot)
- Aktuelle Mounts die NICHT in fstab stehen (tmpfs, cgroups etc.) als separate Sektion
- Eintrag editieren: Dialog mit Feldern + Options-Checkboxen (noatime, discard, compress etc.)
- Neuen Eintrag hinzufuegen: Device-Auswahl via UDisks2 (Dropdown mit erkannten Partitionen)
- Validierung vor dem Schreiben:
  - Mountpoint existiert (oder anlegen anbieten)
  - Device existiert / UUID ist gueltig
  - Keine doppelten Mountpoints
- Apply: Backup -> Schreiben -> `systemctl daemon-reload`

**QML:** `ScrollViewKCM` mit TableView + Edit-Dialog (Kirigami.OverlaySheet)

---

### Modul 5: Kernel-Module

**KCM-Name:** `kcm_fcse_kernelmodules`
**Privilege:** KAuth-Helper (`org.kde.fcse.modprobe.write`, `org.kde.fcse.modprobe.load`)

**Features:**
- Liste geladener Module aus `/proc/modules`: Name, Groesse, Used-by, Status
- Suchfeld mit Filter
- Detail-Panel pro Modul: `modinfo`-Output (Beschreibung, Autor, Version, Parameter, Abhaengigkeiten, Alias)
- Modul-Parameter anzeigen (aus `/sys/module/{name}/parameters/`)
- Aktionen:
  - Modul entladen (`rmmod` via Helper) -- mit Warnung bei Abhaengigkeiten
  - Modul laden (`modprobe` via Helper)
  - Modul blacklisten -> schreibt nach `/etc/modprobe.d/fcse-blacklist.conf`
  - Blacklist aufheben
- Blacklist-Ansicht: alle Eintraege aus `/etc/modprobe.d/*.conf` mit `blacklist` Direktive
- Warnung bei kritischen Modulen (Dateisystem-Treiber, GPU-Treiber)

**QML:** `ScrollViewKCM` mit ListView + Detail-Sidebar

---

### Modul 6: Scheduled Tasks

**KCM-Name:** `kcm_fcse_scheduledtasks`
**Privilege:** D-Bus (`org.freedesktop.systemd1`) + KAuth-Helper (`org.kde.fcse.cron.write`)

**Features:**
- **Tab 1: systemd Timer**
  - Liste aller .timer-Units (System + User)
  - Spalten: Name, Naechste Ausfuehrung, Letzte Ausfuehrung, Intervall
  - Enable/Disable via D-Bus
  - Timer-Details: OnCalendar/OnBootSec/OnUnitActiveSec, zugehoerige .service-Unit
- **Tab 2: Cron Jobs**
  - User-Crontab (`crontab -l`) editierbar ohne Root
  - System-Crontabs (`/etc/cron.d/`, `/etc/crontab`) read-only anzeigen, editieren via KAuth
  - Cron-Ausdruck-Builder: Visuelles Formular (Minute, Stunde, Tag, Monat, Wochentag) mit Live-Vorschau der naechsten 5 Ausfuehrungen
- Neuen Timer erstellen:
  - Wizard: Name, Kommando, Zeitplan (Calendar/Monotonic), Service-Typ
  - Generiert .timer + .service Unit-Files
  - Installiert via D-Bus (User) oder KAuth (System)

**QML:** `SimpleKCM` mit TabBar + Kirigami.OverlaySheet fuer Wizards

---

### Modul 7: Sicherheit

**KCM-Name:** `kcm_fcse_security`
**Privilege:** KAuth-Helper (`org.kde.fcse.sudoers.write`)

**Features:**
- **Tab 1: sudoers**
  - Liste aller Dateien in `/etc/sudoers.d/`
  - Pro Datei: Regeln anzeigen mit menschenlesbarer Erklaerung
  - Neue Regel hinzufuegen: Formular (User/Gruppe, Host, Kommandos, NOPASSWD toggle)
  - Validierung via `visudo -c -f` vor dem Schreiben (KRITISCH -- ungueltige sudoers sperrt sudo aus)
  - `/etc/sudoers` selbst ist read-only (zu gefaehrlich zum Editieren)
- **Tab 2: SELinux**
  - Aktueller Modus: Enforcing / Permissive / Disabled (via `getenforce`)
  - Modus umschalten (Runtime: `setenforce`, Persistent: `/etc/selinux/config`)
  - Letzte 50 AVC-Denials aus dem Audit-Log
  - Pro Denial: Erklaerung + vorgeschlagener Fix (`audit2allow`-Output)
  - Boolean-Toggles: Liste aller SELinux-Booleans mit Beschreibung und Toggle
- **Tab 3: polkit**
  - Liste aktiver polkit-Rules aus `/etc/polkit-1/rules.d/` und `/usr/share/polkit-1/rules.d/`
  - Read-only mit Erklaerung (polkit-Rules sind JavaScript -- zu komplex zum visuell editieren)
  - Installierte .policy-Dateien aus `/usr/share/polkit-1/actions/` auflisten

**QML:** `SimpleKCM` mit TabBar

---

### Modul 8: DNF Repos

**KCM-Name:** `kcm_fcse_dnfrepos`
**Privilege:** KAuth-Helper (`org.kde.fcse.repos.write`)

**Features:**
- Liste aller .repo-Dateien aus `/etc/yum.repos.d/`
- Pro Repo-Sektion: Name, BaseURL/Metalink, Enabled-Toggle, GPG-Check, GPG-Key
- Enable/Disable per Toggle (schreibt `enabled=1/0`)
- COPR-Integration:
  - "COPR hinzufuegen"-Dialog: Owner/Projekt eingeben
  - Fuehrt `dnf5 copr enable owner/project` via KAuth aus
  - COPR-Repos visuell markiert
- Neues Repo hinzufuegen: Formular (Name, URL, GPG-Key optional)
- Repo loeschen (mit Bestaetigung)
- GPG-Key-Trust anzeigen (importierte Keys aus RPM-DB)
- Metadaten-Cache: "Refresh" Button (`dnf5 makecache` via Helper)

**QML:** `ScrollViewKCM` mit ListView + Kirigami.OverlaySheet fuer Details

---

### Modul 9: Hostname & Netzwerk-Identitaet

**KCM-Name:** `kcm_fcse_hostname`
**Privilege:** D-Bus (`org.freedesktop.hostname1`) + KAuth-Helper (`org.kde.fcse.hosts.write`)

**Features:**
- **Hostname-Sektion:**
  - Pretty Hostname, Static Hostname, Transient Hostname (via `org.freedesktop.hostname1`)
  - Icon-Name, Chassis-Typ, Deployment
  - Machine-ID anzeigen (read-only)
- **Hosts-Sektion:**
  - Tabelle aus `/etc/hosts`: IP, Hostname(s)
  - Eintraege hinzufuegen, editieren, loeschen
  - Validierung: IP-Format, keine Duplikate
- **DNS-Sektion:**
  - Aktueller DNS-Status via `resolvectl status`
  - Systemd-resolved Konfiguration (`/etc/systemd/resolved.conf`)
  - DNS-Server, Fallback-DNS, DNSSEC-Modus, DNS-over-TLS
  - Editieren via KAuth-Helper + `systemctl restart systemd-resolved`

**QML:** `SimpleKCM` mit Kirigami.FormLayout Sektionen

---

### Modul 10: Umgebungsvariablen

**KCM-Name:** `kcm_fcse_environment`
**Privilege:** KAuth-Helper (`org.kde.fcse.env.write`) fuer System-Level, unprivilegiert fuer User-Level

**Features:**
- **Tab 1: Systemweit**
  - `/etc/environment` Key=Value Editor
  - `/etc/profile.d/` Skripte auflisten (read-only Inhalt, aktivieren/deaktivieren via KAuth)
- **Tab 2: User-Session**
  - `~/.config/environment.d/*.conf` (systemd user environment generators)
  - Key=Value Editor, braucht kein Root
  - Aenderungen werden nach `~/.config/environment.d/fcse.conf` geschrieben
- Aktive Umgebung anzeigen: Alle aktuellen Variablen aus `/proc/self/environ` als Referenz
- Bekannte Variablen mit Erklaerung markieren (PATH, HOME, XDG_*, LANG, etc.)

**QML:** `SimpleKCM` mit TabBar + TableView

---

### Modul 11: Swap & zram

**KCM-Name:** `kcm_fcse_swap`
**Privilege:** KAuth-Helper (`org.kde.fcse.zram.write`)

**Features:**
- **zram-Sektion:**
  - Aktueller Status: Groesse, Algorithmus, genutzt/frei (aus `/sys/block/zram0/`)
  - Konfiguration editieren:
    - Groesse (Slider: 0.5x bis 2x RAM, oder "min(ram, 8192)" Syntax)
    - Kompressionsalgorithmus: lzo-rle, zstd, lz4, lzo (aus `/sys/block/zram0/comp_algorithm`)
    - Streams-Anzahl
  - Schreibt nach `/etc/systemd/zram-generator.conf`
  - Aenderungen benoetigen Reboot (zram wird beim Boot erstellt)
- **Swap-Sektion:**
  - Aktive Swap-Bereiche aus `/proc/swaps`: Device, Typ, Groesse, Used, Prioritaet
  - Korrelation mit fstab-Eintraegen anzeigen
  - `vm.swappiness` Anzeige mit Link zum sysctl-Modul
- **Statistiken:**
  - Visueller Balken: RAM-Nutzung vs Swap/zram-Nutzung
  - Kompressionsrate (original_data_size vs compressed_data_size aus zram stats)

**QML:** `SimpleKCM` mit Kirigami.FormLayout + Progress-Bars

---

### Modul 12: System-Logs (Journal)

**KCM-Name:** `kcm_fcse_journal`
**Privilege:** Unprivilegiert (User ist in `systemd-journal` Gruppe)

**Features:**
- ListView mit Log-Eintraegen, farbkodiert nach Prioritaet:
  - Emergency/Alert/Critical: rot
  - Error: orange
  - Warning: gelb
  - Notice/Info: normal
  - Debug: grau
- **Filter:**
  - Boot-Auswahl (aktueller Boot, vorheriger, alle)
  - Prioritaet (Minimum-Level Dropdown)
  - Unit-Filter (Autocomplete aus bekannten Units)
  - Zeitraum (Von-Bis Datepicker)
  - Freitext-Suche
- **Kernel-Tab:** Nur Kernel-Messages (`_TRANSPORT=kernel`, entspricht dmesg)
- Live-Tail: Auto-Scroll bei neuen Eintraegen (togglebar)
- Export: Ausgewaehlten Zeitraum als Textdatei speichern
- Technisch: `sd-journal` C-API via `systemd-devel` fuer performantes Lesen

**QML:** `ScrollViewKCM` mit ListView + Filter-Header

---

## 4. Shared Library: libfcse-common

### BackupManager

```cpp
class BackupManager {
public:
    // Erstellt Backup in /var/lib/fcse/backups/
    // Format: {basename}.{ISO-timestamp}.bak
    // Returns: Pfad zum Backup oder Fehler
    static Result<QString> backup(const QString &filePath);

    // Stellt Backup wieder her (kopiert zurueck, erstellt vorher Backup vom aktuellen Stand)
    static Result<void> restore(const QString &backupPath, const QString &originalPath);

    // Listet alle Backups fuer eine Datei, neueste zuerst
    static QList<BackupEntry> listBackups(const QString &filePath);

    // Loescht alte Backups (behaelt maxCount neueste)
    static void rotate(const QString &filePath, int maxCount = 10);
};
```

Automatische Rotation: Max 10 Backups pro Datei. Aeltere werden beim naechsten Backup geloescht.

### DBusHelper

```cpp
class DBusHelper {
public:
    // Async D-Bus Call mit einheitlichem Error-Handling
    static QDBusPendingCall asyncSystemCall(
        const QString &service,
        const QString &path,
        const QString &interface,
        const QString &method,
        const QVariantList &args = {},
        bool interactiveAuth = true
    );

    // Convenience fuer haeufige Services
    static QDBusInterface systemd1Manager();
    static QDBusInterface hostname1();
    static QDBusInterface udisks2Manager();
};
```

### ConfigParser

```cpp
class ConfigParser {
public:
    // sysctl.d: key = value Format
    static QMap<QString, QString> parseSysctl(const QString &filePath);
    static bool writeSysctl(const QString &filePath, const QMap<QString, QString> &values);

    // GRUB: KEY=VALUE / KEY="VALUE" Format
    static QMap<QString, QString> parseGrubDefault(const QString &filePath);
    static bool writeGrubDefault(const QString &filePath, const QMap<QString, QString> &values);

    // fstab: 6-Spalten whitespace-getrennt
    static QList<FstabEntry> parseFstab(const QString &filePath);
    static bool writeFstab(const QString &filePath, const QList<FstabEntry> &entries);

    // BLS: key whitespace value Format
    static BLSEntry parseBLSEntry(const QString &filePath);
    static bool writeBLSEntry(const QString &filePath, const BLSEntry &entry);

    // modprobe.d: blacklist/options/install Direktiven
    static QList<ModprobeDirective> parseModprobeConf(const QString &filePath);
    static bool writeModprobeConf(const QString &filePath, const QList<ModprobeDirective> &directives);

    // .repo: INI-aehnliches Format
    static QList<RepoSection> parseRepoFile(const QString &filePath);
    static bool writeRepoFile(const QString &filePath, const QList<RepoSection> &sections);

    // hosts: IP hostname [aliases...] Format
    static QList<HostsEntry> parseHosts(const QString &filePath);
    static bool writeHosts(const QString &filePath, const QList<HostsEntry> &entries);

    // environment: KEY=VALUE Format
    static QMap<QString, QString> parseEnvironment(const QString &filePath);
    static bool writeEnvironment(const QString &filePath, const QMap<QString, QString> &values);

    // zram-generator.conf: INI-Format
    static ZramConfig parseZramGenerator(const QString &filePath);
    static bool writeZramGenerator(const QString &filePath, const ZramConfig &config);

    // resolved.conf: INI-Format
    static ResolvedConfig parseResolved(const QString &filePath);
    static bool writeResolved(const QString &filePath, const ResolvedConfig &config);
};
```

### StatusNotifier

```cpp
class StatusNotifier {
public:
    static void success(const QString &message);     // Gruene KNotification
    static void error(const QString &message);       // Rote KNotification
    static void backupCreated(const QString &path);  // "Backup erstellt: {path}"
    static void warning(const QString &message);     // Gelbe KNotification
};
```

---

## 5. KAuth-Helper: fcse-helper

### Aktionen

Alle Aktionen folgen dem Schema: Argumente validieren -> Backup erstellen -> Datei schreiben -> Post-Action ausfuehren -> Ergebnis zurueckgeben.

```
org.kde.fcse.sysctl.write
  Args: { "content": "key = value\n..." }
  Post: sysctl --system
  Writes: /etc/sysctl.d/99-fcse.conf

org.kde.fcse.grub.write
  Args: { "content": "KEY=VALUE\n..." }
  Post: grub2-mkconfig -o /boot/grub2/grub.cfg
  Writes: /etc/default/grub

org.kde.fcse.bls.read
  Args: { }
  Returns: { "entries": [...] }
  Note: Lesen benoetigt Root wegen /boot/loader/entries/ Permissions

org.kde.fcse.bls.write
  Args: { "filename": "...", "content": "..." }
  Post: (keine)
  Writes: /boot/loader/entries/{filename}

org.kde.fcse.bls.setdefault
  Args: { "kernel": "6.19.10-200.fc43.x86_64" }
  Post: grubby --set-default /boot/vmlinuz-{kernel}

org.kde.fcse.fstab.write
  Args: { "content": "..." }
  Post: systemctl daemon-reload
  Writes: /etc/fstab
  Validates: findmnt --verify --tab-file {temp}

org.kde.fcse.modprobe.write
  Args: { "content": "..." }
  Post: depmod -a
  Writes: /etc/modprobe.d/fcse-blacklist.conf

org.kde.fcse.modprobe.load
  Args: { "module": "...", "action": "load|unload" }
  Post: modprobe {module} | rmmod {module}

org.kde.fcse.cron.write
  Args: { "filename": "...", "content": "..." }
  Writes: /etc/cron.d/{filename}

org.kde.fcse.sudoers.write
  Args: { "filename": "...", "content": "..." }
  Validates: Schreibt temp-Datei, prueft mit visudo -c -f {temp}
  Writes: /etc/sudoers.d/{filename} (nur bei erfolgreicher Validierung)

org.kde.fcse.selinux.setmode
  Args: { "mode": "enforcing|permissive", "persistent": true|false }
  Post: setenforce {0|1} + optional /etc/selinux/config

org.kde.fcse.selinux.setbool
  Args: { "name": "...", "value": true|false, "persistent": true|false }
  Post: setsebool [-P] {name} {on|off}

org.kde.fcse.repos.write
  Args: { "filename": "...", "content": "..." }
  Writes: /etc/yum.repos.d/{filename}

org.kde.fcse.repos.copr
  Args: { "action": "enable|disable", "project": "owner/name" }
  Post: dnf5 copr {enable|disable} {project} -y

org.kde.fcse.hosts.write
  Args: { "content": "..." }
  Writes: /etc/hosts

org.kde.fcse.env.write
  Args: { "file": "environment|profile.d/{name}", "content": "..." }
  Writes: /etc/environment | /etc/profile.d/{name}

org.kde.fcse.resolved.write
  Args: { "content": "..." }
  Post: systemctl restart systemd-resolved
  Writes: /etc/systemd/resolved.conf

org.kde.fcse.zram.write
  Args: { "content": "..." }
  Writes: /etc/systemd/zram-generator.conf
  Note: Benoetigt Reboot (Info an User)

org.kde.fcse.generic.backup
  Args: { "filePath": "..." }
  Returns: { "backupPath": "..." }

org.kde.fcse.generic.restore
  Args: { "backupPath": "...", "originalPath": "..." }
```

### polkit-Policy

Alle Aktionen: `auth_admin` (erfordert Admin-Passwort). Kein NOPASSWD im Design -- das ist eine User-Entscheidung ausserhalb unserer Kontrolle.

```xml
<policyconfig>
  <vendor>Fedora Core Setting Extension</vendor>
  <vendor_url>https://github.com/fedora-core-setting-extension</vendor_url>
  <icon_name>preferences-system</icon_name>

  <action id="org.kde.fcse.sysctl.write">
    <description>Write sysctl configuration</description>
    <message>Authentication is required to modify kernel parameters</message>
    <defaults>
      <allow_any>auth_admin</allow_any>
      <allow_inactive>auth_admin</allow_inactive>
      <allow_active>auth_admin</allow_active>
    </defaults>
  </action>
  <!-- ... analog fuer alle anderen Aktionen -->
</policyconfig>
```

---

## 6. Sicherheitskonzept

### Backup-vor-Schreiben (obligatorisch)

Jede Schreiboperation im KAuth-Helper erstellt ZUERST ein Backup:

```
1. backup(/etc/fstab) -> /var/lib/fcse/backups/fstab.2026-04-07T14:30:00.bak
2. Validierung der neuen Datei
3. Bei Validierungsfehler: Abbruch, Backup bleibt, Fehler an User
4. Bei Erfolg: Datei schreiben
5. Post-Action ausfuehren
6. Bei Post-Action-Fehler: Automatische Wiederherstellung aus Backup
```

### Input-Validierung

| Datei | Validierungsmethode |
|-------|---------------------|
| sudoers | `visudo -c -f {temp}` -- lehnt syntaktisch ungueltige Regeln ab |
| fstab | `findmnt --verify --tab-file {temp}` Syntax- und Plausibilitaetspruefung |
| GRUB | `grub2-script-check` auf generierte Config |
| sysctl | Typ-Pruefung (Integer/String) + Range-Check fuer bekannte Keys |
| modprobe | Modulname gegen `/lib/modules/$(uname -r)/` pruefen |
| hosts | IP-Format-Validierung + Hostname RFC-Konformitaet |
| zram-generator | INI-Syntax + Groessen-Plausibilitaet |

### Was wir NICHT tun

- `/etc/sudoers` direkt editieren -- nur Dateien in `/etc/sudoers.d/`
- Kernel-Module entladen die aktiv genutzt werden (Used-by > 0) -- nur mit expliziter Warnung
- fstab-Eintraege die das Root-Filesystem betreffen (`/`) sind schreibgeschuetzt -- zu gefaehrlich
- SELinux komplett deaktivieren (`disabled`) -- nur Enforcing <-> Permissive Toggle

---

## 7. Projekt-Setup

### Verzeichnis

```
~/fedora-core-setting-extension/     # Git-Repo, spaeter auf GitHub
```

### Build & Test Workflow

```bash
# Build
cd ~/fedora-core-setting-extension
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build

# Test einzelnes KCM (ohne System-Install)
QT_PLUGIN_PATH=build/kcms kcmshell6 kcm_fcse_services

# System-Install (fuer Integration in Systemsettings)
sudo cmake --install build

# Deinstallation
sudo xargs rm < build/install_manifest.txt
```

### Build-Dependencies (Fedora 43)

```bash
sudo dnf5 install \
  extra-cmake-modules \
  kf6-kcmutils-devel \
  kf6-kauth-devel \
  kf6-ki18n-devel \
  kf6-kcoreaddons-devel \
  kf6-kconfig-devel \
  kf6-kconfigwidgets-devel \
  kf6-kservice-devel \
  kf6-kirigami-devel \
  kf6-kdbusaddons-devel \
  qt6-qtbase-devel \
  qt6-qtdeclarative-devel \
  systemd-devel \
  cmake \
  gcc-c++
```

### Runtime-Dependencies

- `grubby`, `grub2-tools` (Bootloader-Modul)
- `kmod` (Kernel-Module-Modul)
- `zram-generator` (Swap-Modul)
- `audit` (SELinux-Denials lesen)
- `policycoreutils` (`setenforce`, `setsebool`)
- `dnf5` + `dnf5-plugins` (COPR-Support)
- `polkit`, `polkit-kde-agent-1` (Authentifizierung)

### SELinux-Kompatibilitaet

Alle installierten Dateien landen in Standard-Pfaden, die SELinux bereits kennt:
- `/usr/lib64/qt6/plugins/` -- `lib_t`
- `/usr/libexec/kf6/kauth/` -- `bin_t`
- `/usr/share/polkit-1/actions/` -- `usr_t`
- `/usr/share/dbus-1/` -- `usr_t`
- `/var/lib/fcse/` -- braucht evtl. eigenen SELinux-Context, oder `var_lib_t` reicht

---

## 8. Implementierungsreihenfolge

Empfohlene Build-Reihenfolge nach Abhaengigkeiten:

| Phase | Was | Warum zuerst |
|-------|-----|-------------|
| **Phase 0** | Projekt-Skeleton + CMake + Kategorie-Registration | Grundgeruest |
| **Phase 1** | libfcse-common (BackupManager, DBusHelper, ConfigParser) | Alle Module haengen davon ab |
| **Phase 2** | KAuth-Helper mit Grundstruktur + polkit-Policy | Alle schreibenden Module brauchen den |
| **Phase 3a** | Journal-Modul | Kein KAuth noetig, gut zum Testen der QML-Pipeline |
| **Phase 3b** | systemd-Services-Modul | D-Bus direkt, kein KAuth, zweiter Test der Pipeline |
| **Phase 4** | sysctl-Modul | Erstes Modul mit KAuth, einfache Key-Value Config |
| **Phase 5** | Hostname-Modul | Mix aus D-Bus + KAuth |
| **Phase 6** | Bootloader-Modul | Komplex, BLS + GRUB |
| **Phase 7** | fstab-Modul | Komplex, UDisks2 + KAuth |
| **Phase 8** | Kernel-Module, DNF-Repos, Environment, Swap, Scheduled Tasks | Mittlere Komplexitaet |
| **Phase 9** | Sicherheit (sudoers + SELinux + polkit) | Am sensibelsten, zuletzt |
| **Phase 10** | Polish: Notifications, Presets, Export, i18n | Feinschliff |

---

## 9. Offene Entscheidungen

Keine -- alle wesentlichen Fragen wurden im Brainstorming geklaert.
