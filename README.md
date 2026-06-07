# ClamSecurity

Interfaz gráfica nativa para Linux que integra **ClamAV** y **UFW** en una sola aplicación — antivirus, firewall, protección en tiempo real y actualizaciones automáticas de firmas, todo desde un panel unificado.

> Desarrollado con Qt6/C++ · Probado en Manjaro KDE Plasma (Wayland/X11)

---

## Características

- **Panel de estado** con indicador visual tri-estado (verde / ámbar / rojo) y checklist detallado de salud del sistema
- **Escaneo flexible** — carpeta personalizada, Home completo, o un archivo/carpeta concreto desde el menú contextual de Dolphin
- **Protección en Tiempo Real** con `clamonacc` (fanotify) — detecta amenazas en el momento en que se crean o modifican
- **On-Access Prevention** — bloquea la ejecución de archivos infectados antes de que sean abiertos
- **Cuarentena** — los archivos sospechosos se mueven, ofuscan y catalogan; restauración o eliminación con un clic
- **Firewall UFW** — habilita/deshabilita y gestiona reglas sin tocar la terminal
- **Actualizaciones automáticas** de firmas con `freshclam`; configura la frecuencia en horas desde la interfaz
- **Amenazas activas** — historial de detecciones con fecha, ruta y acciones disponibles
- **Exclusiones** — lista de carpetas y extensiones excluidas del escaneo
- **Programador** — escaneos automáticos con cron sin escribir una sola línea
- **System Tray** con indicador de protección e inicio oculto al arrancar el sistema
- **Sin root** — los comandos privilegiados usan `pkexec` (diálogo Polkit estándar de KDE)
- Soporte de idiomas: **Español** e **Inglés** (detección automática del sistema)

---

## Capturas de pantalla

![Vista general — equipo protegido](docs/images/overview-protected.png)
![Detalles de estado del sistema](docs/images/details.png)
![Módulo de escaneo](docs/images/scan.png)
![Base de datos y actualizaciones automáticas](docs/images/database.png)
![Gestión del firewall UFW](docs/images/firewall.png)
![System Tray](docs/images/tray.png)

---

## Requisitos previos — ejecuta `setup.sh` primero

Antes de compilar o ejecutar ClamSecurity por primera vez, ejecuta el script de configuración incluido. Realiza automáticamente todo lo necesario para que ClamAV funcione correctamente en tu sistema:

```bash
chmod +x setup.sh
./setup.sh
```

### ¿Qué hace `setup.sh`?

| Paso | Descripción |
|---|---|
| **Detección de distro** | Identifica Arch/Manjaro, Debian/Ubuntu o Fedora y usa el gestor de paquetes correcto (`pacman` / `apt` / `dnf`) |
| **Instala dependencias** | `clamav`, `acl`, `ufw` y los paquetes auxiliares necesarios para cada distro |
| **Comenta `Example`** | En Arch/Manjaro los archivos `clamd.conf` y `freshclam.conf` traen una directiva `Example` que impide que los servicios arranquen; el script la comenta automáticamente |
| **Configura `clamd.conf`** | Añade una sección delimitada al final del archivo con las opciones de monitoreo en tiempo real (`OnAccessIncludePath`, `OnAccessPrevention`, exclusiones para `.cache` y `Trash`) apuntando al Home del usuario |
| **Override de clamonacc** | Crea `/etc/systemd/system/clamav-clamonacc.service.d/override.conf` con los flags `-F` y `--log=…` para evitar que el servicio intente escribir la cuarentena en `/root/quarantine` |
| **ACLs** | Otorga a `clamav` permiso de solo lectura sobre el Home del usuario mediante `setfacl`, sin añadirlo al grupo personal ni modificar permisos estándar. Las ACLs por defecto (`-d`) garantizan que los archivos nuevos también sean accesibles |
| **inotify** | Escribe `/etc/sysctl.d/99-clamsecurity.conf` para aumentar `fs.inotify.max_user_watches` a 524 288. El límite por defecto (8 192) se agota en Homes con muchos archivos anidados, dejando a ClamAV sin cobertura en directorios profundos |
| **Actualiza firmas** | Ejecuta `freshclam` para descargar la base de datos inicial de virus |
| **Habilita servicios** | Activa `clamav-daemon`, `clamav-clamonacc` (si existe) y UFW |

---

## Dependencias de compilación

```bash
# Qt6 y herramientas de build
sudo pacman -S qt6-base qt6-tools cmake ninja base-devel

# Iconos Breeze (recomendado en entornos no-KDE)
sudo pacman -S breeze-icons

# polkit (normalmente ya presente en KDE)
sudo pacman -S polkit
```

En Debian/Ubuntu:

```bash
sudo apt-get install qt6-base-dev qt6-tools-dev cmake ninja-build build-essential libpolicykit-1-dev
```

En Fedora:

```bash
sudo dnf install qt6-qtbase-devel qt6-qttools-devel cmake ninja-build gcc-c++ polkit-devel
```

---

## Compilación

```bash
# Desde la raíz del proyecto
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja
```

El binario quedará en `build/ClamSecurity`. Puedes ejecutarlo directamente sin instalar.

### Instalar en el sistema (opcional)

```bash
sudo ninja install
```

### Abrir en Qt Creator

1. `File → Open File or Project` → selecciona `CMakeLists.txt`
2. Kit: **Qt 6.x (Desktop)**
3. `Configure Project` → `Build → Build All` (Ctrl+B)

---

## Ejecución

```bash
# Normal
./build/ClamSecurity

# Escanear una ruta directamente
./build/ClamSecurity --scan /home/usuario/Descargas

# Arrancar oculto en el System Tray
./build/ClamSecurity --hidden
```

---

## Service Menu de Dolphin (clic derecho)

### Desde la GUI

1. Abre ClamSecurity → **Configuración**
2. Pulsa **"Instalar Service Menu de Dolphin"**
3. Reinicia Dolphin

### Manual

```bash
mkdir -p ~/.local/share/kio/servicemenus/
cp scripts/org.kde.clamsecurity.desktop ~/.local/share/kio/servicemenus/
chmod 644 ~/.local/share/kio/servicemenus/org.kde.clamsecurity.desktop
kquitapp6 dolphin; dolphin &
```

> Si el binario no está en el `PATH`, edita `Exec=` en el `.desktop` con la ruta completa.

---

## Inicio automático con el sistema

### Desde la GUI

Configuración → Inicio del Sistema → activa **"Iniciar con el sistema"**.

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

## Notas sobre la Protección en Tiempo Real

### Override de clamonacc

Sin configuración adicional, `clamonacc` puede intentar guardar la cuarentena en `/root/quarantine`. El override que crea `setup.sh` fuerza el flag `-F` (foreground, necesario para systemd) y especifica el log explícitamente:

```ini
# /etc/systemd/system/clamav-clamonacc.service.d/override.conf
[Service]
ExecStart=
ExecStart=/usr/sbin/clamonacc -F --log=/var/log/clamav/clamonacc.log
```

Puedes editarlo manualmente con `sudo systemctl edit clamav-clamonacc.service`.

### ACLs — acceso de lectura para clamav

`clamonacc` necesita leer el Home del usuario para monitorear archivos en tiempo real. La forma más segura de concederle acceso es mediante ACLs:

```bash
sudo setfacl -m u:clamav:X /home/$USER          # traversal en Home
sudo setfacl -R -m u:clamav:rX /home/$USER      # lectura del contenido actual
sudo setfacl -R -d -m u:clamav:rX /home/$USER   # herencia para archivos nuevos
```

Esto no añade `clamav` al grupo personal del usuario ni modifica los permisos POSIX estándar.

### Límite de inotify

Linux limita por defecto el número de watches de inotify a 8 192. Cuando ClamAV monitorea un Home con muchos archivos y directorios anidados, este límite se agota y deja de recibir eventos en los directorios más profundos.

```bash
# Ver el límite actual
cat /proc/sys/fs/inotify/max_user_watches

# Aumentarlo de forma permanente (lo hace setup.sh automáticamente)
echo 'fs.inotify.max_user_watches=524288' | sudo tee /etc/sysctl.d/99-clamsecurity.conf
sudo sysctl --system
```

---

## Estructura del proyecto

```
ClamSecurity/
├── setup.sh                            ← Configuración inicial del sistema
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── mainwindow.h / .cpp
│   ├── managers/
│   │   ├── ClamAvManager               ← clamscan / daemon / freshclam / clamonacc
│   │   ├── ClamdConfigManager          ← Lectura/escritura de clamd.conf
│   │   ├── FreshclamConfigManager      ← Frecuencia de actualización en freshclam.conf
│   │   ├── QuarantineManager           ← Cuarentena en ~/.local/share/ClamSecurity/
│   │   ├── UFWManager                  ← Control de UFW vía pkexec
│   │   ├── SystemChecker               ← Estado global tri-estado
│   │   ├── SchedulerManager            ← Escaneos programados con cron
│   │   └── AutostartManager            ← ~/.config/autostart/
│   ├── workers/
│   │   └── ScanWorker                  ← clamscan en QThread
│   ├── core/
│   │   └── NotificationService         ← D-Bus + journal watcher
│   └── ui/
│       ├── MonitorWidget               ← Indicador visual (verde/ámbar/rojo)
│       ├── ModuleCard                  ← Tarjetas del panel principal
│       ├── DetailsDialog               ← Checklist de estado detallado
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
│   └── org.kde.clamsecurity.desktop    ← Service Menu para Dolphin
├── translations/
│   ├── ClamSecurity_es.ts
│   └── ClamSecurity_en.ts
└── docs/
    └── images/                         ← Capturas de pantalla
```

---

## Seguridad y privilegios

- La aplicación **nunca** se ejecuta como root.
- Los comandos privilegiados (`systemctl`, `ufw`, escritura en `/etc/clamav/`) usan `pkexec`, que muestra el diálogo de autenticación Polkit estándar de KDE.
- El escaneo (`clamscan`) se ejecuta como el usuario actual — solo accede a archivos que el usuario ya puede leer.
- La cuarentena se almacena en `~/.local/share/ClamSecurity/quarantine/`. Los archivos se ofuscan con XOR para evitar ejecución accidental y se indexan en JSON.

---

## Resolución de problemas

| Problema | Solución |
|---|---|
| Monitor en **ROJO** tras instalar | Ejecuta `setup.sh` o inicia los servicios: `sudo systemctl enable --now clamav-daemon` |
| `clamonacc` no inicia | Revisa `journalctl -u clamav-clamonacc` — probablemente falta el override o las ACLs |
| Cuarentena en `/root/quarantine` | Aplica el override de clamonacc (ver sección anterior o ejecuta `setup.sh`) |
| Service Menu no aparece en Dolphin | Reinstálalo desde Configuración o verifica que el binario esté en el `PATH` |
| Firmas muy antiguas | Usa "Actualizar Ahora" en Base de Datos o ejecuta `sudo freshclam` |
| `pkexec` no encuentra el programa | Instala con `sudo ninja install` o usa la ruta absoluta |
| Notificaciones no llegan | Verifica que `OnAccessPrevention yes` esté en `clamd.conf` y que `clamonacc` esté activo |
| RT activa pero monitor en **ámbar** | On-Access Prevention está desactivado — actívalo desde Ajustes |
| inotify exhausted en logs | Aumenta `fs.inotify.max_user_watches` (lo hace `setup.sh` automáticamente) |

---

## Licencia

Copyright (C) 2026 Josué Carrasco

Este programa es software libre: puedes redistribuirlo y/o modificarlo bajo los términos de la **GNU General Public License** publicada por la Free Software Foundation, ya sea la versión 3 de la Licencia, o (a tu elección) cualquier versión posterior.

Este programa se distribuye con la esperanza de que sea útil, pero **SIN NINGUNA GARANTÍA**. Consulta el archivo [LICENSE](LICENSE) para más detalles.
