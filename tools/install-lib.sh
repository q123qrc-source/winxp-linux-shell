# SPDX-License-Identifier: GPL-3.0-or-later
# Shared by install.sh and uninstall.sh (sourced, bash).
#
# The manifest (<prefix>/share/meadow/install-manifest.txt) records, with
# absolute paths on the target system:
#   option KEY=VALUE        how install.sh was run (prefix, pam, greeter, linked)
#   dir PATH                a directory install.sh created
#   file SHA256 PATH        an installed file
#   link PATH TARGET        an installed symlink
#   backup PATH             PATH existed before and was saved as PATH.meadow-bak
# Changes outside the prefix are not in it: meadow-setup records those in its
# own state file and reverts them.

MANIFEST_REL=share/meadow/install-manifest.txt

say() { printf '%s\n' "$*"; }
warn() { printf 'warning: %s\n' "$*" >&2; }
die() { printf '%s: %s\n' "${0##*/}" "$*" >&2; exit "${2:-1}"; }

# run CMD...: print it; run it unless --dry-run.
run() {
    say "+ $*"
    [ "${dry_run:-0}" = 1 ] || "$@"
}

# The target-system path for an absolute path P under the install root.
on_target() { printf '%s%s' "${dest%/}" "$1"; }

sha() { sha256sum "$1" | cut -d' ' -f1; }

# Distribution family from <root>/etc/os-release (same mapping as meadow-setup).
distro_family() {
    local root=$1 id="" like=""
    if [ -r "$root/etc/os-release" ]; then
        id=$(. "$root/etc/os-release" && printf '%s' "${ID:-}")
        like=$(. "$root/etc/os-release" && printf '%s' "${ID_LIKE:-}")
    fi
    case " $id $like " in
        *" fedora "*|*" rhel "*|*" centos "*) echo fedora ;;
        *" debian "*|*" ubuntu "*) echo debian ;;
        *" arch "*) echo arch ;;
        *) echo other ;;
    esac
}

# Package names per dependency: "fedora|arch|debian".
declare -A PKG=(
    [cmake]="cmake|cmake|cmake"
    [ninja]="ninja-build|ninja|ninja-build"
    [c++]="gcc-c++|gcc|g++"
    [python3]="python3|python|python3"
    [pkg-config]="pkgconf-pkg-config|pkgconf|pkgconf"
    [ecm]="extra-cmake-modules|extra-cmake-modules|extra-cmake-modules"
    [qt6]="qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtdeclarative-devel qt6-qtwayland-devel qt6-qtmultimedia-devel qt6-qttools-devel qt6-qtsvg|qt6-base qt6-declarative qt6-wayland qt6-multimedia qt6-tools qt6-svg|qt6-base-dev qt6-base-private-dev qt6-declarative-dev qt6-wayland-dev qt6-multimedia-dev qt6-tools-dev qt6-svg-dev qml6-module-qtquick qml6-module-qtquick-templates qml6-module-qtquick-window qt6-svg-plugins"
    [layer-shell-qt]="layer-shell-qt-devel|layer-shell-qt|liblayershellqtinterface-dev"
    [polkit-qt6]="polkit-qt6-1-devel|polkit-qt6|libpolkit-qt6-1-dev"
    [wayland]="wayland-devel wayland-protocols-devel|wayland wayland-protocols|libwayland-dev wayland-protocols"
    [pam]="pam-devel|pam|libpam0g-dev"
    [rsvg-convert]="librsvg2-tools|librsvg|librsvg2-bin"
    [xcursorgen]="xcursorgen|xorg-xcursorgen|x11-apps"
    [dconf]="dconf|dconf|dconf-cli"
    [labwc]="labwc|labwc|labwc"
    [sddm]="sddm|sddm|sddm"
    [kvantum]="kvantum|kvantum|qt6-style-kvantum"
    [pcmanfm-qt]="pcmanfm-qt|pcmanfm-qt|pcmanfm-qt"
)

pkg_for() {
    local family=$1 key=$2 f a d
    IFS='|' read -r f a d <<<"${PKG[$key]}"
    case $family in
        fedora) echo "$f" ;;
        arch) echo "$a" ;;
        debian) echo "$d" ;;
        *) echo "($key)" ;;
    esac
}

install_hint() {
    local family=$1; shift
    local pkgs="" k
    for k in "$@"; do pkgs="$pkgs $(pkg_for "$family" "$k")"; done
    case $family in
        fedora) echo "sudo dnf install$pkgs" ;;
        arch) echo "sudo pacman -S --needed$pkgs" ;;
        debian) echo "sudo apt install$pkgs" ;;
        *) echo "install:$pkgs" ;;
    esac
}

have_cmake_package() {
    local name=$1 d
    for d in /usr/lib64/cmake /usr/lib/cmake /usr/lib/*-linux-gnu/cmake /usr/share/cmake /usr/local/lib*/cmake; do
        [ -d "$d/$name" ] && return 0
    done
    return 1
}

# check_deps FAMILY: report missing build (fatal) and runtime (warning) deps.
check_deps() {
    local family=$1 missing=() runtime=() qtv
    command -v cmake >/dev/null || missing+=(cmake)
    command -v ninja >/dev/null || command -v ninja-build >/dev/null || command -v make >/dev/null || missing+=(ninja)
    command -v c++ >/dev/null || command -v g++ >/dev/null || command -v clang++ >/dev/null || missing+=(c++)
    command -v python3 >/dev/null || missing+=(python3)
    command -v pkg-config >/dev/null || missing+=(pkg-config)
    have_cmake_package ECM || [ -d /usr/share/ECM ] || missing+=(ecm)
    if qtv=$(qt_query QT_VERSION); then
        if [ "$(printf '%s\n6.5\n' "$qtv" | sort -V | head -1)" != 6.5 ]; then
            warn "Qt $qtv found; Meadow needs Qt 6.5 or newer"
            missing+=(qt6)
        fi
    else
        missing+=(qt6)
    fi
    have_cmake_package LayerShellQt || missing+=(layer-shell-qt)
    if command -v pkg-config >/dev/null; then
        pkg-config --exists polkit-qt6-core-1 2>/dev/null || have_cmake_package PolkitQt6-1 || missing+=(polkit-qt6)
        pkg-config --exists wayland-client wayland-protocols 2>/dev/null || missing+=(wayland)
        # wlr-protocols is optional: shell/protocols has the two files needed.
    fi
    [ -e /usr/include/security/pam_appl.h ] || missing+=(pam)
    command -v rsvg-convert >/dev/null || missing+=(rsvg-convert)
    command -v xcursorgen >/dev/null || missing+=(xcursorgen)
    command -v dconf >/dev/null || runtime+=(dconf)
    command -v labwc >/dev/null || runtime+=(labwc)
    command -v sddm >/dev/null || runtime+=(sddm)
    [ -d /usr/share/Kvantum ] || command -v kvantummanager >/dev/null || runtime+=(kvantum)
    command -v pcmanfm-qt >/dev/null || runtime+=(pcmanfm-qt)
    if [ ${#runtime[@]} -gt 0 ]; then
        warn "not installed (needed to run a Meadow session): ${runtime[*]}"
        say "  $(install_hint "$family" "${runtime[@]}")"
    fi
    if [ ${#missing[@]} -gt 0 ]; then
        say "Missing build dependencies: ${missing[*]}" >&2
        say "  $(install_hint "$family" "${missing[@]}")" >&2
        return 1
    fi
    return 0
}

# qt_query VAR: a qtpaths/qmake query (QT_VERSION, QT_INSTALL_QML).
qt_query() {
    local tool
    for tool in qtpaths6 qtpaths-qt6 qmake6 qmake-qt6 /usr/lib/qt6/bin/qtpaths /usr/lib/qt6/bin/qmake; do
        if command -v "$tool" >/dev/null 2>&1; then
            case $tool in
                *qtpaths*) "$tool" --query "$1" 2>/dev/null && return 0 ;;
                *) "$tool" -query "$1" 2>/dev/null && return 0 ;;
            esac
        fi
    done
    return 1
}

cache_var() { sed -n "s/^$2:[A-Z]*=//p" "$1/CMakeCache.txt" | head -1; }
