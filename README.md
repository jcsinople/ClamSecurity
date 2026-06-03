# ClamSecurity

Interfaz gráfica nativa para Linux (Manjaro/KDE Plasma Wayland) que integra **ClamAV** y **UFW** en una sola aplicación con diseño limpio tipo Kaspersky.

---

## Dependencias del sistema

Instala todo desde los repositorios oficiales con `pacman`:

```bash
# Qt6 y herramientas de compilación
sudo pacman -S qt6-base qt6-tools cmake ninja base-devel

# ClamAV (antivirus)
sudo pacman -S clamav

# UFW (firewall)
sudo pacman -S ufw

# polkit (para pkexec — normalmente ya instalado en KDE)
sudo pacman -S polkit

# Opcional: iconos Breeze para que los iconos se vean correctamente
sudo pacman -S breeze-icons
```

---

## Configuración inicial de ClamAV

```bash
# 1. Actualizar la base de datos de firmas por primera vez
sudo freshclam

# 2. Habilitar e iniciar los servicios
sudo systemctl enable --now clamav-daemon
sudo systemctl enable --now clamav-freshclam

# 3. Verificar que están activos
systemctl is-active clamav-daemon
systemctl is-active clamav-freshclam
```

---

## Configuración inicial de UFW

```bash
# Habilitar UFW con política por defecto (bloquear entrante, permitir saliente)
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw enable

# Verificar estado
sudo ufw status verbose
```

---

## Compilación

```bash
# 1. Clonar / entrar al directorio del proyecto
cd /ruta/a/ClamSecurity

# 2. Crear directorio de build
mkdir build && cd build

# 3. Configurar con CMake (Release para producción)
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja

# 4. Compilar (usa todos los núcleos disponibles)
ninja

# 5. (Opcional) Instalar en el sistema
sudo ninja install
```

El binario `ClamSecurity` quedará en `build/ClamSecurity`. Puedes ejecutarlo directamente sin instalar.

---

## Abrir en Qt Creator

1. Abre **Qt Creator**.
2. Ve a `File → Open File or Project`.
3. Selecciona el archivo `CMakeLists.txt` en la raíz del proyecto.
4. Qt Creator detecta automáticamente los kits de Qt6. Selecciona el kit **Qt 6.x (Desktop)**.
5. Pulsa **Configure Project**.
6. Usa **Build → Build All** (Ctrl+B) o el botón ▶ para compilar y ejecutar.

---

## Ejecución

```bash
# Ejecutar normalmente
./build/ClamSecurity

# Escanear directamente desde la línea de comandos (equivale al Service Menu)
./build/ClamSecurity --scan /home/usuario/Descargas

# Arrancar oculto en el System Tray
./build/ClamSecurity --hidden
```

---

## Service Menu de Dolphin (clic derecho)

### Opción A — Desde la GUI
1. Abre **ClamSecurity**.
2. Ve a **Configuración**.
3. Pulsa el botón **"Instalar Service Menu de Dolphin"**.
4. Reinicia Dolphin.

### Opción B — Manual

```bash
# Crear directorio de service menus del usuario
mkdir -p ~/.local/share/kio/servicemenus/

# Copiar el archivo incluido en el proyecto
cp scripts/org.kde.clamsecurity.desktop \
   ~/.local/share/kio/servicemenus/

# Asignar permisos correctos
chmod 644 ~/.local/share/kio/servicemenus/org.kde.clamsecurity.desktop

# Reiniciar Dolphin (o cerrar y volver a abrir)
kquitapp6 dolphin; dolphin &
```

### Verificar que funciona

1. Abre Dolphin.
2. Haz clic derecho sobre cualquier archivo o carpeta.
3. Debe aparecer la opción **"Escanear con ClamSecurity"**.
4. Al hacer clic, se abrirá ClamSecurity y comenzará el escaneo de ese elemento.

> **Nota:** Si el binario `ClamSecurity` no está en el `PATH` del sistema, edita el campo `Exec=` del archivo `.desktop` con la ruta completa, por ejemplo: `Exec=/home/usuario/Cloud/.../build/ClamSecurity --scan %f`

---

## Inicio automático con el sistema

### Desde la GUI
1. Ve a **Configuración → Inicio del Sistema**.
2. Activa **"Iniciar ClamSecurity con el sistema"**.
3. Opcionalmente activa **"Arrancar oculto en el System Tray"**.

### Manual
```bash
mkdir -p ~/.config/autostart/

cat > ~/.config/autostart/ClamSecurity.desktop << 'EOF'
[Desktop Entry]
Type=Application
Name=ClamSecurity
Comment=Antivirus y Firewall para Linux
Exec=ClamSecurity
Icon=security-high
Terminal=false
Categories=System;Security;
X-GNOME-Autostart-enabled=true
EOF
```

---

## Estructura del proyecto

```
ClamSecurity/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                    ← Punto de entrada, args --scan / --hidden
│   ├── mainwindow.h / .cpp         ← Ventana principal, tray, navegación
│   ├── managers/
│   │   ├── ClamAvManager           ← Interacción con clamscan / daemon / freshclam
│   │   ├── QuarantineManager       ← Cuarentena en ~/.local/share/ClamSecurity/
│   │   ├── UFWManager              ← Control de UFW vía pkexec
│   │   ├── SystemChecker           ← Estado global, refresco periódico
│   │   └── AutostartManager        ← ~/.config/autostart/ClamSecurity.desktop
│   ├── workers/
│   │   └── ScanWorker              ← clamscan en QThread, señales de progreso
│   ├── core/
│   │   └── NotificationService     ← D-Bus notifications + journal watcher
│   └── ui/
│       ├── MonitorWidget           ← Monitor QPainter (verde/rojo)
│       ├── ModuleCard              ← Tarjetas clickables del menú principal
│       ├── DetailsDialog           ← Checklist de estado detallado
│       ├── ScanPage                ← Módulo de escaneo
│       ├── DatabasePage            ← Actualización de firmas
│       ├── ExclusionsPage          ← Gestión de exclusiones
│       ├── QuarantinePage          ← Cuarentena (tabla + restaurar/eliminar)
│       ├── FirewallPage            ← Control UFW
│       └── SettingsPage            ← Configuración y autostart
├── resources/
│   ├── icons/clamsecurity.svg      ← Icono SVG de la app
│   └── ClamSecurity.qrc
└── scripts/
    └── org.kde.clamsecurity.desktop ← Service Menu para Dolphin
```

---

## Seguridad y privilegios

- **Nunca** se ejecuta con root. Los comandos que necesitan permisos elevados usan `pkexec`, que abre el diálogo estándar de autenticación de Polkit de KDE.
- Los comandos con `pkexec` son: `systemctl start/stop clamav-daemon`, `ufw enable/disable`, `freshclam`.
- El escaneo (`clamscan`) se ejecuta como el usuario normal — solo puede leer archivos que el usuario ya puede leer.
- La cuarentena se almacena en `~/.local/share/ClamSecurity/quarantine/`. Los archivos se ofuscan con XOR (evita ejecución accidental) y se indexan en JSON.

---

## Resolución de problemas

| Problema | Solución |
|----------|----------|
| "ClamAV no instalado" | `sudo pacman -S clamav && sudo freshclam` |
| Monitor en ROJO tras instalar | Inicia los servicios: `sudo systemctl enable --now clamav-daemon` |
| Service Menu no aparece en Dolphin | Reinstálalo desde Configuración o comprueba que el binario esté en el PATH |
| `pkexec` no encuentra el programa | Asegúrate de que `/usr/bin/ClamSecurity` existe (`sudo ninja install`) |
| Firmas muy antiguas | Ejecuta `sudo freshclam` manualmente o usa el botón "Actualizar Ahora" |
| Notificaciones no llegan | Verifica que `clamav-daemon` tenga OnAccess configurado en `/etc/clamav/clamd.conf` |
