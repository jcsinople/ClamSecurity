# ClamSecurity

Interfaz gráfica nativa para Linux que integra **ClamAV** y **UFW** en una sola aplicación — antivirus, firewall, protección en tiempo real y actualizaciones automáticas de firmas, todo desde un panel unificado.

<p align="center">
  <img src="docs/images/overview-protected.png" alt="Vista general — equipo protegido" />
</p>

> Desarrollado con Qt6/C++ · Probado en Manjaro KDE Plasma (Wayland)

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

## Instalación — ejecuta `setup.sh`

Un solo comando instala las dependencias, compila la aplicación, la registra en el sistema y configura ClamAV correctamente:

```bash
chmod +x setup.sh
./setup.sh
```

Al finalizar, ClamSecurity aparecerá en el menú de aplicaciones bajo la categoría **Sistema / Seguridad**.

### ¿Qué hace `setup.sh`?

| Paso | Descripción |
|---|---|
| **Detecta la distro** | Identifica Arch/Manjaro, Debian/Ubuntu o Fedora y usa el gestor de paquetes correcto (`pacman` / `apt` / `dnf`) |
| **Dependencias de ejecución** | Instala `clamav`, `acl`, `ufw` y paquetes auxiliares |
| **Dependencias de compilación** | Instala Qt6, cmake, ninja y herramientas de build por distro |
| **Compila ClamSecurity** | Ejecuta `cmake` + `ninja` desde la raíz del proyecto en modo Release |
| **Instala en el sistema** | `sudo ninja install` + actualiza la base de datos de escritorio y el caché de iconos; en KDE ejecuta `kbuildsycoca6` para que aparezca en el menú |
| **Comenta `Example`** | En Arch/Manjaro `clamd.conf` y `freshclam.conf` traen una directiva `Example` que impide arrancar los servicios; el script la comenta |
| **Configura `clamd.conf`** | Añade una sección delimitada al final con `OnAccessIncludePath` apuntando a la carpeta de descargas del usuario (detectada por XDG, independiente del idioma), `OnAccessPrevention yes`, y exclusiones para archivos parcialmente descargados (`.crdownload`, `.part`) |
| **Override de clamonacc** | Crea `/etc/systemd/system/clamav-clamonacc.service.d/override.conf` con los flags `-F` y `--log=…` para evitar que el servicio escriba cuarentena en `/root/quarantine` |
| **ACLs** | Otorga a `clamav` permiso de solo lectura sobre el Home del usuario con `setfacl`, sin añadirlo al grupo personal ni modificar permisos POSIX. Las ACLs por defecto (`-d`) aplican a archivos y carpetas nuevos |
| **inotify** | Escribe `/etc/sysctl.d/99-clamsecurity.conf` para aumentar `fs.inotify.max_user_watches` a 524 288 (el límite por defecto, 8 192, se agota en Homes con muchos archivos anidados) |
| **Actualiza firmas** | Descarga la base de datos inicial de virus con `freshclam` |
| **Habilita servicios** | Activa `clamav-daemon`, `clamav-clamonacc` (si existe) y UFW |

---

## Módulos del panel principal

El panel principal muestra ocho tarjetas de acceso rápido organizadas en dos filas:

| Módulo | Descripción |
|---|---|
| **Escanear** | Inicia un escaneo bajo demanda sobre el Home, una carpeta personalizada o cualquier ruta. Muestra progreso en tiempo real, estadísticas y archivos infectados. Los archivos detectados pueden enviarse directamente a cuarentena. |
| **Base de Datos** | Muestra la versión y fecha de las firmas instaladas. Actualiza manualmente con "Actualizar Ahora" o configura actualizaciones automáticas en segundo plano (servicio `freshclam`) con frecuencia ajustable en horas. |
| **Exclusiones** | Gestiona carpetas y extensiones que se ignorarán durante los escaneos y la protección en tiempo real. Los cambios se sincronizan automáticamente con `clamd.conf`. |
| **Ajustes** | Tres pestañas: **General** (inicio automático, tema, idioma, control de servicios, Service Menu de Dolphin) · **Protección** (OnAccessPrevention, carpetas monitoreadas, parámetros avanzados de `clamonacc`) · **Acerca de** (versión, repositorio, verificación de actualizaciones). |
| **Cuarentena** | Lista los archivos aislados con nombre, ruta original y fecha de detección. Permite restaurar un archivo o eliminarlo permanentemente. Los archivos en cuarentena no afectan el estado de protección del panel. |
| **Firewall** | Controla UFW sin usar la terminal: activa o desactiva el firewall, visualiza las reglas activas, y añade o elimina reglas por protocolo y puerto. |
| **Amenazas Activas** | Historial de archivos detectados como amenaza. Muestra ruta, fecha y estado. Permite actuar sobre cada detección: enviar a cuarentena, eliminar o marcar como revisado. |
| **Programador** | Configura escaneos automáticos con temporizadores systemd de usuario. Define la carpeta objetivo, la frecuencia y visualiza los próximos escaneos programados. |

---

## Compilación manual (opcional)

Si prefieres compilar sin ejecutar `setup.sh`, necesitas instalar primero las dependencias de build:

```bash
# Arch/Manjaro
sudo pacman -S qt6-base qt6-tools cmake ninja base-devel polkit breeze-icons

# Debian/Ubuntu
sudo apt-get install qt6-base-dev qt6-tools-dev cmake ninja-build build-essential libpolicykit-1-dev

# Fedora
sudo dnf install qt6-qtbase-devel qt6-qttools-devel cmake ninja-build gcc-c++ polkit-devel
```

```bash
# Desde la raíz del proyecto
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja
```

El binario quedará en `build/ClamSecurity`. Puedes ejecutarlo directamente sin instalar.

### Instalar en el sistema

```bash
sudo ninja install

# Registrar el .desktop y el icono
sudo update-desktop-database /usr/local/share/applications/
sudo gtk-update-icon-cache -f /usr/local/share/icons/hicolor/ 2>/dev/null || true
kbuildsycoca6   # solo en KDE — actualiza el menú de aplicaciones
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
├── setup.sh                            ← Instalación y configuración completa
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
| No aparece en el menú de aplicaciones | Ejecuta `kbuildsycoca6` o cierra sesión y vuelve a entrar |
| Service Menu no aparece en Dolphin | Reinstálalo desde Configuración o verifica que el binario esté en el `PATH` |
| Firmas muy antiguas | Usa "Actualizar Ahora" en Base de Datos o ejecuta `sudo freshclam` |
| `pkexec` no encuentra el programa | Instala con `sudo ninja install` o usa la ruta absoluta |
| Notificaciones no llegan | Verifica que `OnAccessPrevention yes` esté en `clamd.conf` y que `clamonacc` esté activo |
| RT activa pero monitor en **ámbar** | On-Access Prevention está desactivado — actívalo desde Ajustes |
| inotify exhausted en logs | Aumenta `fs.inotify.max_user_watches` (lo hace `setup.sh` automáticamente) |

---

## Capturas de pantalla

<p align="center">
  <img src="docs/images/details.png" alt="Detalles de estado del sistema" />
</p>
<p align="center">
  <img src="docs/images/scan.png" alt="Módulo de escaneo" />
</p>
<p align="center">
  <img src="docs/images/database.png" alt="Base de datos y actualizaciones automáticas" />
</p>
<p align="center">
  <img src="docs/images/firewall.png" alt="Gestión del firewall UFW" />
</p>
<p align="center">
  <img src="docs/images/tray.png" alt="System Tray" />
</p>

---

## Licencia

Copyright (C) 2026 Josué Carrasco

Este programa es software libre: puedes redistribuirlo y/o modificarlo bajo los términos de la **GNU General Public License versión 3** publicada por la Free Software Foundation.

Este programa se distribuye con la esperanza de que sea útil, pero **SIN NINGUNA GARANTÍA**. Consulta el archivo [LICENSE](LICENSE) para más detalles.
