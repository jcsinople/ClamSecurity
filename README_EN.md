# ClamSecurity

Native Linux GUI that integrates **ClamAV** and **UFW** into a single application — antivirus, firewall, real-time protection and automatic signature updates, all from a unified dashboard.

> Built with Qt6/C++ · Tested on Manjaro KDE Plasma (Wayland/X11)

---

## Features

- **Status dashboard** with tri-state visual indicator (green / amber / red) and a detailed system health checklist
- **Flexible scanning** — custom folder, full Home, or a specific file/folder via Dolphin's context menu
- **Real-Time Protection** with `clamonacc` (fanotify) — detects threats the moment files are created or modified
- **On-Access Prevention** — blocks execution of infected files before they are opened
- **Quarantine** — suspicious files are moved, obfuscated and catalogued; restore or delete with one click
- **UFW Firewall** — enable/disable and manage rules without opening a terminal
- **Automatic signature updates** with `freshclam`; configure update frequency in hours from the UI
- **Active Threats** — detection history with date, path and available actions
- **Exclusions** — list of folders and extensions excluded from scanning
- **Scheduler** — automated scans with cron without writing a single line
- **System Tray** with protection indicator and hidden startup on login
- **No root required** — privileged commands use `pkexec` (standard KDE Polkit dialog)
- Languages: **Spanish** and **English** (auto-detected from system locale)

---

## Screenshots

![Overview — fully protected](docs/images/overview-protected.png)
![System status details](docs/images/details.png)
![Scan module in progress](docs/images/scan.png)
![Database and automatic updates](docs/images/database.png)
![UFW firewall management](docs/images/firewall.png)
![System Tray](docs/images/tray.png)

---

## Installation — run `setup.sh`

A single command installs all dependencies, compiles the application, registers it on the system and configures ClamAV correctly:

```bash
chmod +x setup.sh
./setup.sh
```

When finished, ClamSecurity will appear in the application menu under **System / Security**.

### What does `setup.sh` do?

| Step | Description |
|---|---|
| **Detect distro** | Identifies Arch/Manjaro, Debian/Ubuntu or Fedora and uses the correct package manager (`pacman` / `apt` / `dnf`) |
| **Runtime dependencies** | Installs `clamav`, `acl`, `ufw` and auxiliary packages |
| **Build dependencies** | Installs Qt6, cmake, ninja and build tools per distro |
| **Compile ClamSecurity** | Runs `cmake` + `ninja` from the project root in Release mode |
| **Install system-wide** | `sudo ninja install` + updates desktop database and icon cache; on KDE runs `kbuildsycoca6` to register in the app menu |
| **Comment out `Example`** | On Arch/Manjaro, `clamd.conf` and `freshclam.conf` ship with an `Example` directive that prevents services from starting; the script comments it out |
| **Configure `clamd.conf`** | Appends a delimited section with `OnAccessIncludePath` pointing to the user's Downloads folder (detected via XDG, locale-aware), `OnAccessPrevention yes`, and exclusions for partially downloaded files (`.crdownload`, `.part`) |
| **clamonacc override** | Creates `/etc/systemd/system/clamav-clamonacc.service.d/override.conf` with the `-F` and `--log=…` flags to prevent the service from writing quarantine to `/root/quarantine` |
| **ACLs** | Grants `clamav` read-only access to the user's Home via `setfacl`, without adding it to the personal group or changing POSIX permissions. Default ACLs (`-d`) apply to new files and folders automatically |
| **inotify** | Writes `/etc/sysctl.d/99-clamsecurity.conf` to raise `fs.inotify.max_user_watches` to 524 288 (the default of 8 192 is quickly exhausted in deeply nested Home directories) |
| **Update signatures** | Downloads the initial virus database with `freshclam` |
| **Enable services** | Activates `clamav-daemon`, `clamav-clamonacc` (if present) and UFW |

---

## Main Panel Modules

The main panel shows eight quick-access cards arranged in two rows:

| Module | Description |
|---|---|
| **Scan** | Starts an on-demand scan of the Home directory, a custom folder, or any path. Displays real-time progress, statistics and detected files. Infected files can be sent directly to quarantine. |
| **Database** | Shows the version and date of the installed ClamAV signatures. Update manually with "Update Now" or configure background automatic updates (freshclam service) with an adjustable frequency in hours. |
| **Exclusions** | Manages folders and extensions that will be skipped during scans and real-time protection. Changes are automatically synchronized with `clamd.conf`. |
| **Settings** | Three tabs: **General** (autostart, theme, language, service controls, Dolphin Service Menu) · **Protection** (OnAccessPrevention, monitored folders, advanced `clamonacc` parameters) · **About** (version, repository, update check). |
| **Quarantine** | Lists isolated files with their original name, path and detection date. Allows restoring a file to its original location or permanently deleting it. Quarantined files do not affect the protection status on the dashboard. |
| **Firewall** | Controls UFW without the terminal: enable or disable the firewall, view active rules, and add or remove rules by protocol and port. |
| **Active Threats** | History of files flagged as threats. Shows path, detection date and status. Allows acting on each detection: send to quarantine, delete or mark as reviewed. |
| **Scheduler** | Configures automatic scans using systemd user timers. Define the target folder, frequency and view upcoming scheduled scans. |

---

## Manual build (optional)

If you prefer to build without running `setup.sh`, first install the build dependencies:

```bash
# Arch/Manjaro
sudo pacman -S qt6-base qt6-tools cmake ninja base-devel polkit breeze-icons

# Debian/Ubuntu
sudo apt-get install qt6-base-dev qt6-tools-dev cmake ninja-build build-essential libpolicykit-1-dev

# Fedora
sudo dnf install qt6-qtbase-devel qt6-qttools-devel cmake ninja-build gcc-c++ polkit-devel
```

```bash
# From the project root
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja
```

The binary will be at `build/ClamSecurity`. You can run it directly without installing.

### Install system-wide

```bash
sudo ninja install

# Register the .desktop entry and icon
sudo update-desktop-database /usr/local/share/applications/
sudo gtk-update-icon-cache -f /usr/local/share/icons/hicolor/ 2>/dev/null || true
kbuildsycoca6   # KDE only — refreshes the application menu
```

### Open in Qt Creator

1. `File → Open File or Project` → select `CMakeLists.txt`
2. Kit: **Qt 6.x (Desktop)**
3. `Configure Project` → `Build → Build All` (Ctrl+B)

---

## Running

```bash
# Normal launch
./build/ClamSecurity

# Scan a specific path
./build/ClamSecurity --scan /home/user/Downloads

# Start hidden in the System Tray
./build/ClamSecurity --hidden
```

---

## Dolphin Service Menu (right-click)

### From the GUI

1. Open ClamSecurity → **Settings**
2. Click **"Install Dolphin Service Menu"**
3. Restart Dolphin

### Manual

```bash
mkdir -p ~/.local/share/kio/servicemenus/
cp scripts/org.kde.clamsecurity.desktop ~/.local/share/kio/servicemenus/
chmod 644 ~/.local/share/kio/servicemenus/org.kde.clamsecurity.desktop
kquitapp6 dolphin; dolphin &
```

> If the binary is not in your `PATH`, edit the `Exec=` field in the `.desktop` file with the full path.

---

## Start with the system

### From the GUI

Settings → System Startup → enable **"Start with the system"**.

### Manual

```bash
mkdir -p ~/.config/autostart/
cat > ~/.config/autostart/ClamSecurity.desktop << 'EOF'
[Desktop Entry]
Type=Application
Name=ClamSecurity
Exec=ClamSecurity
Icon=security-high
Terminal=false
Categories=System;Security;
X-GNOME-Autostart-enabled=true
EOF
```

---

## Notes on Real-Time Protection

### clamonacc override

Without additional configuration, `clamonacc` may try to store quarantine in `/root/quarantine`. The override created by `setup.sh` forces the `-F` flag (foreground, required by systemd) and specifies the log path explicitly:

```ini
# /etc/systemd/system/clamav-clamonacc.service.d/override.conf
[Service]
ExecStart=
ExecStart=/usr/sbin/clamonacc -F --log=/var/log/clamav/clamonacc.log
```

You can also edit it manually with `sudo systemctl edit clamav-clamonacc.service`.

### ACLs — read access for clamav

`clamonacc` needs to read the user's Home to monitor files in real time. The safest way to grant this access is via ACLs:

```bash
sudo setfacl -m u:clamav:X /home/$USER          # traversal permission on Home
sudo setfacl -R -m u:clamav:rX /home/$USER      # read existing content
sudo setfacl -R -d -m u:clamav:rX /home/$USER   # inherit for new files
```

This does not add `clamav` to the user's personal group or change standard POSIX permissions.

### inotify limit

Linux limits the number of inotify watches to 8 192 by default. When ClamAV monitors a Home directory with many nested files and folders, this limit is exhausted and ClamAV stops receiving events from deep subtrees.

```bash
# Check current limit
cat /proc/sys/fs/inotify/max_user_watches

# Increase permanently (done automatically by setup.sh)
echo 'fs.inotify.max_user_watches=524288' | sudo tee /etc/sysctl.d/99-clamsecurity.conf
sudo sysctl --system
```

---

## Project structure

```
ClamSecurity/
├── setup.sh                            ← Full installation and setup
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── mainwindow.h / .cpp
│   ├── managers/
│   │   ├── ClamAvManager               ← clamscan / daemon / freshclam / clamonacc
│   │   ├── ClamdConfigManager          ← clamd.conf read/write
│   │   ├── FreshclamConfigManager      ← Update frequency in freshclam.conf
│   │   ├── QuarantineManager           ← Quarantine in ~/.local/share/ClamSecurity/
│   │   ├── UFWManager                  ← UFW control via pkexec
│   │   ├── SystemChecker               ← Tri-state global status
│   │   ├── SchedulerManager            ← Scheduled scans with cron
│   │   └── AutostartManager            ← ~/.config/autostart/
│   ├── workers/
│   │   └── ScanWorker                  ← clamscan in QThread
│   ├── core/
│   │   └── NotificationService         ← D-Bus + journal watcher
│   └── ui/
│       ├── MonitorWidget               ← Visual indicator (green/amber/red)
│       ├── ModuleCard                  ← Dashboard cards
│       ├── DetailsDialog               ← Detailed status checklist
│       ├── ScanPage
│       ├── DatabasePage
│       ├── ExclusionsPage
│       ├── QuarantinePage
│       ├── FirewallPage
│       ├── SettingsPage
│       ├── ActiveThreatsPage
│       └── ScheduledScansPage
├── resources/
│   ├── icons/clamsecurity.svg
│   └── ClamSecurity.qrc
├── scripts/
│   └── org.kde.clamsecurity.desktop    ← Dolphin Service Menu
├── translations/
│   ├── ClamSecurity_es.ts
│   └── ClamSecurity_en.ts
└── docs/
    └── images/                         ← Screenshots
```

---

## Security and privileges

- The application **never** runs as root.
- Privileged commands (`systemctl`, `ufw`, writing to `/etc/clamav/`) use `pkexec`, which shows the standard KDE Polkit authentication dialog.
- Scanning (`clamscan`) runs as the current user — it can only read files the user already has access to.
- Quarantine is stored in `~/.local/share/ClamSecurity/quarantine/`. Files are XOR-obfuscated to prevent accidental execution and indexed in JSON.

---

## Troubleshooting

| Problem | Solution |
|---|---|
| Monitor shows **RED** after install | Run `setup.sh` or start services: `sudo systemctl enable --now clamav-daemon` |
| `clamonacc` fails to start | Check `journalctl -u clamav-clamonacc` — likely missing override or ACLs |
| Quarantine appears in `/root/quarantine` | Apply the clamonacc override (see above or run `setup.sh`) |
| Not showing in application menu | Run `kbuildsycoca6` or log out and back in |
| Service Menu missing in Dolphin | Reinstall from Settings or verify binary is in `PATH` |
| Signatures out of date | Use "Update Now" on the Database page or run `sudo freshclam` |
| `pkexec` cannot find the program | Install with `sudo ninja install` or use the absolute path |
| Notifications not arriving | Verify `OnAccessPrevention yes` is in `clamd.conf` and `clamonacc` is active |
| RT active but monitor shows **amber** | On-Access Prevention is disabled — enable it in Settings |
| inotify exhausted in logs | Increase `fs.inotify.max_user_watches` (done automatically by `setup.sh`) |

---

## License

Copyright (C) 2026 Josué Carrasco

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License version 3** published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but **WITHOUT ANY WARRANTY**. See the [LICENSE](LICENSE) file for details.
