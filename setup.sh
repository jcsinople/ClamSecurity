#!/usr/bin/env bash
# =============================================================================
#  ClamSecurity — Setup Script
#  Instala dependencias, compila, instala y configura ClamAV correctamente.
#
#  Soporta: Arch/Manjaro · Debian/Ubuntu · Fedora/RHEL
#  Uso:     chmod +x setup.sh && ./setup.sh
# =============================================================================
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'

info()  { echo -e "${GREEN}[✔]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[!]${NC}  $*"; }
error() { echo -e "${RED}[✘]${NC}  $*" >&2; }
step()  { echo -e "\n${CYAN}───  $*${NC}"; }
die()   { error "$*"; exit 1; }

# ── Guardia de root ───────────────────────────────────────────────────────────
[ "$EUID" -eq 0 ] && die "No ejecutes este script como root. Usará sudo cuando sea necesario."

REAL_USER="${SUDO_USER:-$USER}"
USER_HOME=$(eval echo "~$REAL_USER")
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ── Detección de distro ───────────────────────────────────────────────────────
detect_distro() {
    if command -v pacman &>/dev/null; then
        echo "arch"
    elif command -v apt-get &>/dev/null; then
        echo "debian"
    elif command -v dnf &>/dev/null; then
        echo "fedora"
    else
        echo "unknown"
    fi
}

# ── Carpeta de descargas del usuario (detecta el nombre localizado) ───────────
get_downloads_dir() {
    local dir=""
    # xdg-user-dir devuelve la ruta real según el locale del usuario
    if command -v xdg-user-dir &>/dev/null; then
        dir=$(xdg-user-dir DOWNLOAD 2>/dev/null)
    fi
    # Fallback: leer ~/.config/user-dirs.dirs manualmente
    if [ -z "$dir" ] || [ "$dir" = "$USER_HOME" ]; then
        local cfg="$USER_HOME/.config/user-dirs.dirs"
        if [ -f "$cfg" ]; then
            dir=$(grep "^XDG_DOWNLOAD_DIR" "$cfg" \
                  | cut -d= -f2 | tr -d '"' \
                  | sed "s|\\\$HOME|$USER_HOME|g")
        fi
    fi
    echo "${dir:-$USER_HOME/Downloads}"
}

# ── 1. Dependencias en tiempo de ejecución ────────────────────────────────────
install_runtime_deps() {
    local distro="$1"
    step "Instalando dependencias de ClamAV ($distro)..."
    case "$distro" in
        arch)
            sudo pacman -Sy --noconfirm --needed clamav acl ufw
            ;;
        debian)
            sudo apt-get update -qq
            sudo apt-get install -y clamav clamav-daemon clamav-freshclam acl ufw
            ;;
        fedora)
            sudo dnf install -y clamav clamav-update clamd acl ufw
            ;;
        *)
            die "Distribución no soportada. Instala ClamAV y acl manualmente."
            ;;
    esac
    info "Dependencias de ejecución instaladas."
}

# ── 2. Dependencias de compilación ────────────────────────────────────────────
install_build_deps() {
    local distro="$1"
    step "Instalando dependencias de compilación ($distro)..."
    case "$distro" in
        arch)
            sudo pacman -Sy --noconfirm --needed \
                qt6-base qt6-tools cmake ninja base-devel polkit breeze-icons \
                kwindowsystem
            ;;
        debian)
            sudo apt-get install -y \
                qt6-base-dev qt6-tools-dev cmake ninja-build \
                build-essential libpolicykit-1-dev libkf6windowsystem-dev
            ;;
        fedora)
            sudo dnf install -y \
                qt6-qtbase-devel qt6-qttools-devel cmake ninja-build \
                gcc-c++ polkit-devel kf6-kwindowsystem-devel
            ;;
        *)
            die "No se pudieron instalar las dependencias de compilación."
            ;;
    esac
    info "Dependencias de compilación instaladas."
}

# ── 3. Compilar e instalar ClamSecurity ───────────────────────────────────────
build_and_install() {
    [ -f "$SCRIPT_DIR/CMakeLists.txt" ] || \
        die "CMakeLists.txt no encontrado. Ejecuta setup.sh desde la raíz del proyecto."

    step "Compilando ClamSecurity..."
    local build_dir="$SCRIPT_DIR/build"
    mkdir -p "$build_dir"

    cmake -S "$SCRIPT_DIR" -B "$build_dir" \
          -DCMAKE_BUILD_TYPE=Release -G Ninja -Wno-dev
    ninja -C "$build_dir"
    info "Compilación completada."

    step "Instalando ClamSecurity en el sistema..."
    sudo ninja -C "$build_dir" install

    # Registrar .desktop e icono para que aparezca en el menú de aplicaciones
    sudo update-desktop-database /usr/local/share/applications/ 2>/dev/null || true
    sudo gtk-update-icon-cache -f /usr/local/share/icons/hicolor/ 2>/dev/null || true
    command -v kbuildsycoca6 &>/dev/null && kbuildsycoca6 2>/dev/null || true

    info "ClamSecurity instalado y registrado en el menú de aplicaciones."
}

# ── 4. Configuración básica de clamd.conf ─────────────────────────────────────
configure_clamd() {
    local conf="/etc/clamav/clamd.conf"
    step "Configurando $conf..."

    [ -f "$conf" ] || { warn "$conf no existe, omitiendo."; return; }

    # En Arch/Manjaro la directiva 'Example' impide que arranque el servicio
    if grep -q "^Example" "$conf" 2>/dev/null; then
        sudo sed -i 's/^Example/#Example/' "$conf"
        info "Directiva 'Example' comentada en clamd.conf."
    fi

    local downloads_dir; downloads_dir=$(get_downloads_dir)
    info "Carpeta de descargas detectada: $downloads_dir"

    # Eliminar sección anterior de ClamSecurity si existe
    local tmp; tmp=$(mktemp)
    awk '/^# --- BEGIN ClamSecurity ---/{skip=1} \
         !skip{print} \
         /^# --- END ClamSecurity ---/{skip=0}' "$conf" > "$tmp"

    cat >> "$tmp" <<EOF

# --- BEGIN ClamSecurity ---
# Generado por setup.sh. Ajusta desde Ajustes › Protección en la aplicación.
OnAccessPrevention yes
OnAccessMaxFileSize 10M
OnAccessMaxThreads 4
OnAccessDenyOnError no
OnAccessExtraScanning yes
OnAccessIncludePath $downloads_dir
OnAccessExcludePath /dev
OnAccessExcludePath /sys
OnAccessExcludePath /proc
OnAccessExcludeUname clamav
ExcludePath \.crdownload$
ExcludePath \.part$
# --- END ClamSecurity ---
EOF

    sudo cp "$tmp" "$conf"
    rm -f "$tmp"
    info "clamd.conf configurado (OnAccessIncludePath → $downloads_dir)."
}

# ── 5. Configurar freshclam.conf ──────────────────────────────────────────────
configure_freshclam() {
    local conf="/etc/clamav/freshclam.conf"
    step "Configurando $conf..."

    [ -f "$conf" ] || { warn "$conf no existe, omitiendo."; return; }

    if grep -q "^Example" "$conf" 2>/dev/null; then
        sudo sed -i 's/^Example/#Example/' "$conf"
        info "Directiva 'Example' comentada en freshclam.conf."
    fi
}

# ── 6. Override del servicio clamav-clamonacc ─────────────────────────────────
# Sin este override, clamonacc puede escribir la cuarentena en /root/quarantine.
# El flag -F (foreground) es necesario para que systemd gestione el proceso.
setup_clamonacc_override() {
    step "Configurando override de clamav-clamonacc..."

    local bin=""
    bin=$(command -v clamonacc 2>/dev/null || true)
    if [ -z "$bin" ]; then
        for p in /usr/sbin/clamonacc /usr/bin/clamonacc; do
            [ -x "$p" ] && bin="$p" && break
        done
    fi
    [ -z "$bin" ] && { warn "clamonacc no encontrado; omitiendo override."; return; }
    info "clamonacc encontrado en: $bin"

    local override_dir="/etc/systemd/system/clamav-clamonacc.service.d"
    local override_file="$override_dir/override.conf"
    sudo mkdir -p "$override_dir"
    sudo tee "$override_file" > /dev/null <<EOF
[Service]
ExecStart=
ExecStart=$bin -F --log=/var/log/clamav/clamonacc.log
EOF

    sudo systemctl daemon-reload
    info "Override escrito en $override_file."
}

# ── 7. ACLs para que clamav pueda leer el Home sin privilegios de root ────────
# setfacl otorga lectura a clamav sin modificar grupos ni permisos POSIX.
# Las ACLs por defecto (-d) aplican automáticamente a nuevos archivos y carpetas.
setup_acl() {
    step "Configurando ACLs para clamav en $USER_HOME..."

    if ! command -v setfacl &>/dev/null; then
        warn "setfacl no encontrado. Instala el paquete 'acl' y vuelve a ejecutar."
        return
    fi

    sudo setfacl -m u:clamav:X "$USER_HOME"
    sudo setfacl -R -m u:clamav:rX "$USER_HOME"
    sudo setfacl -R -d -m u:clamav:rX "$USER_HOME"

    info "ACLs configuradas. clamav puede leer $USER_HOME."
}

# ── 8. Límite de inotify ──────────────────────────────────────────────────────
# El límite por defecto (8192 watches) se agota al monitorear un Home con
# muchos archivos anidados, dejando sin cobertura los directorios profundos.
setup_inotify() {
    step "Aumentando límite de inotify watches..."
    local sysctl_file="/etc/sysctl.d/99-clamsecurity.conf"
    if [ -f "$sysctl_file" ] && grep -q "max_user_watches" "$sysctl_file"; then
        info "Límite de inotify ya configurado en $sysctl_file."
        return
    fi
    sudo tee "$sysctl_file" > /dev/null <<'EOF'
# ClamSecurity: aumentar watches para monitoreo en tiempo real con inotify
fs.inotify.max_user_watches=524288
fs.inotify.max_user_instances=512
EOF
    sudo sysctl --system -q
    info "inotify max_user_watches establecido en 524288."
}

# ── 9. Actualizar firmas de virus ─────────────────────────────────────────────
update_signatures() {
    step "Descargando firmas de virus (puede tardar unos minutos)..."
    sudo systemctl stop clamav-freshclam 2>/dev/null || true
    if sudo freshclam; then
        info "Firmas actualizadas correctamente."
    else
        warn "freshclam encontró un error. Puedes actualizar manualmente desde la app."
    fi
    sudo systemctl start clamav-freshclam 2>/dev/null || true
}

# ── 10. Habilitar servicios ───────────────────────────────────────────────────
enable_services() {
    step "Habilitando servicios de ClamAV..."

    sudo systemctl enable --now clamav-daemon 2>/dev/null \
        && info "clamav-daemon habilitado e iniciado." \
        || warn "No se pudo habilitar clamav-daemon."

    if systemctl list-unit-files clamav-clamonacc.service &>/dev/null; then
        sudo systemctl enable clamav-clamonacc 2>/dev/null \
            && info "clamav-clamonacc habilitado (se iniciará con el sistema)." \
            || warn "No se pudo habilitar clamav-clamonacc."
    else
        warn "clamav-clamonacc.service no encontrado en esta distribución."
    fi

    if command -v ufw &>/dev/null && ! sudo ufw status | grep -q "Status: active"; then
        sudo ufw default deny incoming
        sudo ufw default allow outgoing
        sudo ufw --force enable
        info "UFW habilitado con política por defecto."
    else
        info "UFW ya estaba activo o no disponible."
    fi
}

# ── Main ──────────────────────────────────────────────────────────────────────
main() {
    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════╗"
    echo -e "║        ClamSecurity — Setup & Instalación           ║"
    echo -e "╚══════════════════════════════════════════════════════╝${NC}"
    echo ""

    local distro; distro=$(detect_distro)
    info "Distribución detectada: $distro"
    info "Usuario: $REAL_USER  │  Home: $USER_HOME"
    info "Directorio del proyecto: $SCRIPT_DIR"

    install_runtime_deps  "$distro"
    install_build_deps    "$distro"
    build_and_install
    configure_clamd
    configure_freshclam
    setup_clamonacc_override
    setup_acl
    setup_inotify
    update_signatures
    enable_services

    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════╗"
    echo -e "║          ¡ClamSecurity instalado y listo!           ║"
    echo -e "╚══════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "  ClamSecurity ya debería aparecer en el menú de aplicaciones."
    echo "  Si no aparece de inmediato, cierra sesión y vuelve a entrar."
    echo ""
    echo "  Primeros pasos:"
    echo "  1. Abre ClamSecurity desde el menú de aplicaciones"
    echo "  2. En Ajustes › Protección ajusta las carpetas a monitorear"
    echo "  3. Activa la Protección en Tiempo Real desde el panel principal"
    echo ""
    warn "La protección en tiempo real requiere soporte fanotify en el kernel."
    warn "Si clamonacc no inicia: journalctl -u clamav-clamonacc"
    echo ""
}

main "$@"
