#!/usr/bin/env bash
# build-deps.sh - Build CwC and its dependencies on Ubuntu 26.04
#
# This script checks for required dependency versions and builds from source
# when the system packages are too old. Run from the cwcwm repo directory.
#
# Usage: ./scripts/build-deps.sh [--install-system-deps] [--build-dir DIR]

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-$(dirname "$(realpath "$0")")/../cwc-deps}"
INSTALL_SYSTEM=false
JOBS=$(nproc)

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_ok()   { echo -e "${GREEN}[OK]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_err()  { echo -e "${RED}[ERROR]${NC} $1"; }
log_info() { echo -e "[INFO] $1"; }

for arg in "$@"; do
    case "$arg" in
        --install-system-deps) INSTALL_SYSTEM=true ;;
        --build-dir=*) BUILD_DIR="${arg#*=}" ;;
        --build-dir) shift; BUILD_DIR="$1" ;;
    esac
done

# ---------------------------------------------------------------------------
# Check pkg-config version >= required
# Returns 0 if installed version satisfies minimum, 1 otherwise
# ---------------------------------------------------------------------------
check_pkg() {
    local pkg="$1" min_ver="$2"
    local cur_ver
    cur_ver=$(pkg-config --modversion "$pkg" 2>/dev/null) || return 1
    if pkg-config --atleast-version="$min_ver" "$pkg" 2>/dev/null; then
        log_ok "$pkg $cur_ver (>= $min_ver)"
        return 0
    else
        log_warn "$pkg $cur_ver found but need >= $min_ver"
        return 1
    fi
}

check_cmd() {
    command -v "$1" &>/dev/null
}

# ---------------------------------------------------------------------------
# System packages
# ---------------------------------------------------------------------------
SYSTEM_PKGS=(
    meson ninja-build cmake g++-14
    wayland-protocols libwayland-dev
    libcairo2-dev libxkbcommon-dev libinput-dev libxxhash-dev
    libluajit-5.1-dev libxcb1-dev xwayland libdrm-dev
    lua-lgi libpango1.0-dev gir1.2-pango-1.0
    libzip-dev librsvg2-dev libtomlplusplus-dev hwdata
    libseat-dev libudev-dev libgbm-dev libgles-dev libegl-dev
    libvulkan-dev glslang-tools liblcms2-dev
    libxcb-dri3-dev libxcb-present-dev libxcb-render-util0-dev
    libxcb-shm0-dev libxcb-xfixes0-dev libxcb-xinput-dev
    libxcb-composite0-dev libxcb-ewmh-dev libxcb-icccm4-dev
    libxcb-res0-dev libxcb-errors-dev
    libffi-dev libexpat1-dev libxml2-dev libliftoff-dev
)

echo "========================================="
echo " CwC Dependency Builder (Pop!_OS/Ubuntu)"
echo "========================================="
echo ""

if "$INSTALL_SYSTEM"; then
    log_info "Installing system packages..."
    sudo apt update
    sudo apt install -y "${SYSTEM_PKGS[@]}"
else
    log_info "Checking for essential build tools..."
    missing=()
    for cmd in meson ninja cmake g++-14 pkg-config; do
        if check_cmd "$cmd"; then
            log_ok "$cmd found"
        else
            log_err "$cmd not found"
            missing+=("$cmd")
        fi
    done
    if [ ${#missing[@]} -gt 0 ]; then
        echo ""
        log_err "Missing build tools. Run with --install-system-deps or:"
        echo "  sudo apt install ${SYSTEM_PKGS[*]}"
        exit 1
    fi
fi

echo ""
log_info "Checking dependency versions..."
echo ""

# Track what needs building
needs_hyprutils=false
needs_hyprlang=false
needs_hyprcursor=false
needs_wayland=false
needs_wayland_protocols=false
needs_libdrm=false
needs_pixman=false
needs_libdisplay_info=false
needs_wlroots=false

check_pkg "hyprutils"        "0.7.1"  || needs_hyprutils=true
check_pkg "hyprlang"         "0.4.2"  || needs_hyprlang=true
check_pkg "hyprcursor"       "0.1.0"  || needs_hyprcursor=true
check_pkg "wayland-server"   "1.24.0" || needs_wayland=true
check_pkg "wayland-protocols" "1.38"  || needs_wayland_protocols=true
check_pkg "libdrm"           "2.4.129" || needs_libdrm=true
check_pkg "pixman-1"         "0.43.0" || needs_pixman=true
check_pkg "libdisplay-info"  "0.2.0"  || needs_libdisplay_info=true
check_pkg "wlroots-0.20"     "0.20.0" || needs_wlroots=true

# hyprutils/hyprlang/hyprcursor chain: if a dep needs building, rebuild dependents
if "$needs_hyprutils"; then
    needs_hyprlang=true
fi
if "$needs_hyprlang"; then
    needs_hyprcursor=true
fi

echo ""

# ---------------------------------------------------------------------------
# Helper: clone-or-update a repo
# ---------------------------------------------------------------------------
clone_or_update() {
    local repo_url="$1" dir="$2" ref="$3"
    if [ -d "$dir" ]; then
        log_info "Updating $dir..."
        git -C "$dir" fetch --tags
    else
        log_info "Cloning $repo_url..."
        git clone "$repo_url" "$dir"
    fi
    git -C "$dir" checkout "$ref" 2>/dev/null || git -C "$dir" checkout "tags/$ref"
}

cmake_build_install() {
    local dir="$1"; shift
    rm -rf "$dir/build"
    cmake -B "$dir/build" -S "$dir" -DCMAKE_CXX_COMPILER=g++-14 "$@"
    cmake --build "$dir/build" -j "$JOBS"
    sudo cmake --install "$dir/build"
    sudo ldconfig
}

meson_build_install() {
    local dir="$1"; shift
    rm -rf "$dir/build"
    meson setup "$dir/build" "$dir" "$@"
    ninja -C "$dir/build" -j "$JOBS"
    sudo ninja -C "$dir/build" install
    sudo ldconfig
}

# ---------------------------------------------------------------------------
# Build from source
# ---------------------------------------------------------------------------
mkdir -p "$BUILD_DIR"

if "$needs_hyprutils"; then
    log_info "Building hyprutils..."
    clone_or_update "https://github.com/hyprwm/hyprutils" "$BUILD_DIR/hyprutils" "v0.7.1"
    cmake_build_install "$BUILD_DIR/hyprutils"
    log_ok "hyprutils installed"
fi

if "$needs_hyprlang"; then
    log_info "Building hyprlang..."
    clone_or_update "https://github.com/hyprwm/hyprlang" "$BUILD_DIR/hyprlang" "v0.6.0"
    cmake_build_install "$BUILD_DIR/hyprlang"
    log_ok "hyprlang installed"
fi

if "$needs_hyprcursor"; then
    log_info "Building hyprcursor..."
    clone_or_update "https://github.com/hyprwm/hyprcursor" "$BUILD_DIR/hyprcursor" "v0.1.13"
    cmake_build_install "$BUILD_DIR/hyprcursor"
    log_ok "hyprcursor installed"
fi

if "$needs_wayland"; then
    log_info "Building wayland..."
    clone_or_update "https://gitlab.freedesktop.org/wayland/wayland.git" "$BUILD_DIR/wayland" "1.25.0"
    meson_build_install "$BUILD_DIR/wayland" -Ddocumentation=false -Dtests=false
    log_ok "wayland installed"
fi

if "$needs_wayland_protocols"; then
    log_info "Building wayland-protocols..."
    clone_or_update "https://gitlab.freedesktop.org/wayland/wayland-protocols.git" "$BUILD_DIR/wayland-protocols" "1.48"
    meson_build_install "$BUILD_DIR/wayland-protocols"
    log_ok "wayland-protocols installed"
fi

if "$needs_libdrm"; then
    log_info "Building libdrm..."
    clone_or_update "https://gitlab.freedesktop.org/mesa/drm.git" "$BUILD_DIR/drm" "libdrm-2.4.131"
    meson_build_install "$BUILD_DIR/drm"
    log_ok "libdrm installed"
fi

if "$needs_pixman"; then
    log_info "Building pixman..."
    clone_or_update "https://gitlab.freedesktop.org/pixman/pixman.git" "$BUILD_DIR/pixman" "pixman-0.43.0"
    meson_build_install "$BUILD_DIR/pixman"
    log_ok "pixman installed"
fi

if "$needs_libdisplay_info"; then
    log_info "Building libdisplay-info..."
    clone_or_update "https://gitlab.freedesktop.org/emersion/libdisplay-info.git" "$BUILD_DIR/libdisplay-info" "0.2.0"
    meson_build_install "$BUILD_DIR/libdisplay-info"
    log_ok "libdisplay-info installed"
fi

if "$needs_wlroots"; then
    log_info "Building wlroots..."
    clone_or_update "https://gitlab.freedesktop.org/wlroots/wlroots.git" "$BUILD_DIR/wlroots" "0.20.0"
    meson_build_install "$BUILD_DIR/wlroots" -Dexamples=false
    log_ok "wlroots installed"
fi

echo ""
echo "========================================="
echo " All dependencies ready!"
echo "========================================="
echo ""
echo "To build CwC:"
echo "  cd $(dirname "$(realpath "$0")")"
echo "  meson setup build"
echo "  ninja -C build"
echo "  sudo ninja -C build install"
echo ""
