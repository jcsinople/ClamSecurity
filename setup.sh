#!/usr/bin/env bash
# =============================================================================
#  ClamSecurity — Setup Script
#  Prepara el sistema para usar ClamSecurity: instala dependencias, configura
#  ClamAV, establece el override de clamonacc, ACLs e inotify.
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

# ── 1. Instalación de paquetes ────────────────────────────────────────────────
install_packages() {
    local distro="$1"
    step "Instalando dependencias ($distro)..."

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
    info "Dependencias instaladas."
}

# ── 2. Configuración básica de clamd.conf ─────────────────────────────────────
configure_clamd() {
    local conf="/etc/clamav/clamd.conf"
    step "Configurando $conf..."

    [ -f "$conf" ] || { warn "$conf no existe, omitiendo."; return; }

    # En Arch/Manjaro el archivo trae 'Example' que impide arrancar el servicio
    if grep -q "^Example" "$conf" 2>/dev/null; then
        sudo sed -i 's/^Example/#Example/' "$conf"
        info "Directiva 'Example' comentada en clamd.conf."
    fi

    # Eliminar/reemplazar sección ClamSecurity si ya existe
    local tmp; tmp=$(mktemp)
    awk '/^# --- BEGIN ClamSecurity ---/{skip=1} !skip{print} /^# --- END ClamSecurity ---/{skip=0}' \
        "$conf" > "$tmp"

    # Añadir sección mínima al final
    cat >> "$tmp" <<EOF

# --- BEGIN ClamSecurity ---
# Generado por setup.sh. Personaliza desde Ajustes > Protección en la app.
OnAccessPrevention yes
OnAccessMaxFileSize 10M
OnAccessMaxThreads 4
OnAccessDenyOnError no
OnAccessExtraScanning yes
OnAccessIncludePath $USER_HOME
OnAccessExcludePath /proc
OnAccessExcludePath /sys
OnAccessExcludePath /dev
OnAccessExcludeUname clamav
ExcludePath ^$USER_HOME/\.cache
ExcludePath ^$USER_HOME/\.local/share/Trash
# --- END ClamSecurity ---
EOF

    sudo cp "$tmp" "$conf"
    rm -f "$tmp"
    info "clamd.conf configurado para monitorear $USER_HOME."
}

# ── 3. Configurar freshclam.conf ──────────────────────────────────────────────
configure_freshclam() {
    local conf="/etc/clamav/freshclam.conf"
    step "Configurando $conf..."

    [ -f "$conf" ] || { warn "$conf no existe, omitiendo."; return; }

    # En Arch/Manjaro también tiene 'Example'
    if grep -q "^Example" "$conf" 2>/dev/null; then
        sudo sed -i 's/^Example/#Example/' "$conf"
        info "Directiva 'Example' comentada en freshclam.conf."
    fi
}

# ── 4. Override del servicio clamav-clamonacc ─────────────────────────────────
# Sin este override, clamonacc puede intentar usar /root como directorio de
# trabajo y escribir la cuarentena en /root/quarantine en lugar de la ruta
# configurada. El flag -F (foreground) y el log explícito corrigen este comportamiento.
setup_clamonacc_override() {
    step "Configurando override de clamav-clamonacc..."

    # Detectar ruta del binario
    local bin
    bin=$(command -v clamonacc 2>/dev/null || echo "")
    if [ -z "$bin" ]; then
        # Rutas comunes según distro
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

# ── 5. ACLs para monitoreo sin ejecutar ClamAV como root ─────────────────────
# clamav necesita permiso de lectura sobre el Home del usuario para que el
# escaneo en tiempo real (clamonacc/fanotify) funcione correctamente.
# Se usan ACLs para otorgar acceso de solo lectura sin añadir clamav al grupo
# personal del usuario ni modificar permisos estándar.
setup_acl() {
    step "Configurando ACLs para clamav en $USER_HOME..."

    # Verificar que el sistema de archivos soporta ACLs
    if ! command -v setfacl &>/dev/null; then
        warn "setfacl no encontrado. Instala el paquete 'acl' y vuelve a ejecutar."
        return
    fi

    # 1. Permiso de traversal en el directorio Home
    sudo setfacl -m u:clamav:X "$USER_HOME"
    # 2. Lectura de todo el contenido actual
    sudo setfacl -R -m u:clamav:rX "$USER_HOME"
    # 3. Herencia automática para archivos y carpetas nuevos
    sudo setfacl -R -d -m u:clamav:rX "$USER_HOME"

    info "ACLs configuradas. clamav puede leer $USER_HOME."
}

# ── 6. Límite de inotify ──────────────────────────────────────────────────────
# El límite por defecto (8192 watches) se agota rápidamente al monitorear
# un Home con muchos archivos/directorios anidados. ClamAV puede dejar de
# detectar cambios en directorios profundos si se supera este límite.
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

# ── 7. Actualizar firmas de virus ─────────────────────────────────────────────
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

# ── 8. Habilitar servicios ────────────────────────────────────────────────────
enable_services() {
    step "Habilitando servicios de ClamAV..."

    # clamav-daemon (motor de escaneo)
    sudo systemctl enable --now clamav-daemon 2>/dev/null \
        && info "clamav-daemon habilitado e iniciado." \
        || warn "No se pudo habilitar clamav-daemon."

    # clamav-clamonacc (protección en tiempo real)
    if systemctl list-unit-files clamav-clamonacc.service &>/dev/null; then
        sudo systemctl enable clamav-clamonacc 2>/dev/null \
            && info "clamav-clamonacc habilitado (se iniciará con el sistema)." \
            || warn "No se pudo habilitar clamav-clamonacc."
    else
        warn "clamav-clamonacc.service no encontrado en esta distribución."
    fi

    # UFW (firewall)
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
    echo -e "║        ClamSecurity — Script de Configuración       ║"
    echo -e "╚══════════════════════════════════════════════════════╝${NC}"
    echo ""

    local distro; distro=$(detect_distro)
    info "Distribución detectada: $distro"
    info "Usuario: $REAL_USER  │  Home: $USER_HOME"

    install_packages     "$distro"
    configure_clamd
    configure_freshclam
    setup_clamonacc_override
    setup_acl
    setup_inotify
    update_signatures
    enable_services

    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════╗"
    echo -e "║                  ¡Configuración lista!              ║"
    echo -e "╚══════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo "  Próximos pasos:"
    echo "  1. Ejecuta ClamSecurity"
    echo "  2. En Ajustes › Protección configura las carpetas a monitorear"
    echo "  3. Activa la Protección en Tiempo Real desde la pantalla principal"
    echo ""
    warn "La protección en tiempo real requiere que el kernel tenga soporte fanotify."
    warn "Si clamonacc no inicia, revisa: journalctl -u clamav-clamonacc"
    echo ""
}

main "$@"
