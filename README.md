# NSA Fedora System Manager

A comprehensive KDE System Settings plugin suite for Fedora Linux system administration. Adds a new **"System Administration"** category to KDE System Settings with 12 modules covering everything from systemd services to kernel parameters.

## Modules

| Module | Description |
|--------|-------------|
| Services | Manage systemd services, timers, sockets (start/stop/enable/disable) |
| Kernel Parameters | View and edit sysctl parameters with presets |
| Bootloader | GRUB2 configuration and BLS kernel entry management |
| Mounts | /etc/fstab editor with mount status |
| Kernel Modules | View loaded modules, blacklist management |
| Scheduled Tasks | systemd timers and cron job management |
| Security | sudoers rules, SELinux mode/booleans, polkit overview |
| Package Repos | DNF repository management with COPR support |
| Hostname & Network | Hostname, /etc/hosts, DNS (systemd-resolved) |
| Environment | System and user environment variables |
| Swap & zram | zram-generator configuration and swap statistics |
| System Logs | Real-time systemd journal viewer with filters |

## Features

- Native KDE integration using Kirigami and KDE Frameworks 6
- Privilege escalation via KAuth/polkit (no running as root)
- Automatic backup before every configuration change
- Input validation (visudo check for sudoers, findmnt for fstab)
- SELinux compatible (standard install paths)
- Fedora-first design (DNF5, BLS boot entries, zram-generator)

## Requirements

- Fedora 40+ with KDE Plasma 6
- KDE Frameworks 6.22+
- Qt 6.8+

## Build

### Install dependencies

```bash
sudo dnf install \
  extra-cmake-modules \
  kf6-kcmutils-devel kf6-kauth-devel kf6-ki18n-devel \
  kf6-kcoreaddons-devel kf6-kconfig-devel kf6-kconfigwidgets-devel \
  kf6-kservice-devel kf6-kirigami-devel kf6-kdbusaddons-devel \
  kf6-knotifications-devel qt6-qtbase-devel qt6-qtdeclarative-devel \
  systemd-devel cmake gcc-c++
```

### Build and install

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

### Uninstall

```bash
sudo xargs rm < build/install_manifest.txt
```

## Architecture

- **12 QML-based KCM modules** (.so plugins)
- **Shared library** (libnsa-fsm-common) with backup manager, config parsers, D-Bus helpers
- **KAuth helper** (nsa-fsm-helper) for privileged file operations
- **polkit policies** for authentication

Services with native polkit support (systemd, hostname1) are accessed directly via D-Bus. File operations use the KAuth helper with automatic backup-before-write.

## Safety

- Every config file write creates a timestamped backup in `/var/lib/nsa-fsm/backups/`
- sudoers changes are validated with `visudo -c` before writing
- Root filesystem mount entries are protected from modification
- Helper commands are whitelisted (no arbitrary command execution)
- SELinux mode can only toggle between Enforcing and Permissive (not Disabled)

## License

GPL-3.0-or-later

## Author

NSA (Samuel)
