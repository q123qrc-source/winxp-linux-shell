#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Generic installer for systems without Meadow packages.
#
# Builds Meadow and installs it under a prefix (default /usr/local). Nothing
# outside the prefix is written by this script: the few system changes go
# through meadow-setup, which shows, backs up and records each one, and which
# uninstall.sh asks to revert them:
#   - link-prefix   session entry, SDDM theme and QML links (prefix != /usr)
#   - enable-lock   the lock screen's PAM service (skipped with --no-pam)
#   - enable-greeter  only with --enable-greeter
# Installing never switches the display manager or edits an existing PAM
# service. On Fedora the RPM packages are the supported way to install.
set -euo pipefail

src=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
. "$src/tools/install-lib.sh"

usage() {
    cat <<EOF
Usage: install.sh [options]

  --prefix DIR        install prefix (default /usr/local)
  --root DIR          install into DIR as if it were / (image builds, tests);
                      meadow-setup then works on DIR too
  --destdir DIR       stage into DIR (also: DESTDIR=DIR); system steps are
                      printed, not run
  --build-dir DIR     build directory (default: build-install in the source tree)
  --no-build          install an existing build of --build-dir
  --no-pam            do not install the lock screen's PAM service (the lock
                      screen then cannot unlock: it refuses to lock)
  --enable-greeter    also make the Meadow theme SDDM's login screen
  --skip-deps         do not check dependencies
  --dry-run           print every step; change nothing
  -h, --help          this help

Undo with uninstall.sh (same --prefix / --root).
EOF
}

prefix=/usr/local
root=""
destdir=${DESTDIR:-}
build_dir=""
no_build=0
no_pam=0
greeter=0
skip_deps=0
dry_run=0
orig_args=("$@")
while [ $# -gt 0 ]; do
    case $1 in
        --prefix) prefix=${2:?}; shift ;;
        --prefix=*) prefix=${1#*=} ;;
        --root) root=${2:?}; shift ;;
        --root=*) root=${1#*=} ;;
        --destdir) destdir=${2:?}; shift ;;
        --destdir=*) destdir=${1#*=} ;;
        --build-dir) build_dir=${2:?}; shift ;;
        --build-dir=*) build_dir=${1#*=} ;;
        --no-build) no_build=1 ;;
        --no-pam) no_pam=1 ;;
        --enable-greeter) greeter=1 ;;
        --skip-deps) skip_deps=1 ;;
        --dry-run) dry_run=1 ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
    shift
done

case $prefix in /*) ;; *) die "--prefix must be absolute" 2 ;; esac
case $prefix in *[[:space:]]*) die "--prefix must not contain spaces" 2 ;; esac
prefix=$(realpath -m "$prefix")
[ -n "$root" ] && [ -n "$destdir" ] && die "use either --root or --destdir" 2
# Where files go, and whether meadow-setup runs (live or --root) or the
# system steps are only printed (staging into DESTDIR for a package).
if [ -n "$root" ]; then
    dest=$(realpath -m "$root"); mode=root
elif [ -n "$destdir" ]; then
    dest=$(realpath -m "$destdir"); mode=staged
else
    dest=/; mode=live
fi
[ "$dest" = / ] && dest=""
build_dir=$(realpath -m "${build_dir:-$src/build-install}")
family=$(distro_family "${dest:-/}")

say "Meadow install: prefix $prefix, ${mode} install${dest:+ into $dest}, distribution family $family"
if [ "$family" = fedora ]; then
    say "Fedora: the RPM packages are the supported way to install Meadow (packaging/fedora)."
    say "This install keeps to $prefix; every change outside it goes through meadow-setup."
    [ "$prefix" = /usr ] && warn "--prefix /usr on Fedora shares files with RPM packages; /usr/local is safer"
fi
[ "$dry_run" = 1 ] && say "Dry run: nothing will be changed."

setup="$prefix/bin/meadow-setup"

# System integration through meadow-setup (run after the files are in place).
system_steps() {
    local setup_cmds=() cmd qml_dir system_qml
    if [ "$prefix" != /usr ]; then
        qml_dir=""
        [ -f "$build_dir/CMakeCache.txt" ] && qml_dir=$(cache_var "$build_dir" MEADOW_QML_INSTALL_DIR)
        system_qml=${MEADOW_SYSTEM_QML_DIR:-$(qt_query QT_INSTALL_QML || echo /usr/lib64/qt6/qml)}
        setup_cmds+=("link-prefix $prefix --qml-dir ${qml_dir:-lib64/qt6/qml} --system-qml-dir $system_qml")
    fi
    [ "$no_pam" = 0 ] && setup_cmds+=("enable-lock")
    [ "$greeter" = 1 ] && setup_cmds+=("enable-greeter")
    for cmd in "${setup_cmds[@]}"; do
        # shellcheck disable=SC2086
        case $mode in
            staged) say "after installing the staged files, run as root:  $setup $cmd" ;;
            root) if [ "$dry_run" = 1 ]; then say "+ $setup --root $dest --yes $cmd"; else
                      say "+ meadow-setup --root $dest $cmd"; python3 "$(on_target "$setup")" --root "$dest" --yes $cmd; fi ;;
            live) run "$setup" --yes $cmd ;;
        esac
    done
}

# 1. Dependencies.
if [ "$skip_deps" = 0 ] && [ "$no_build" = 0 ]; then
    check_deps "$family" || exit 4
fi

# 2. Build (as the invoking user; only the install step needs root).
if [ "$no_build" = 0 ]; then
    generator=()
    command -v ninja >/dev/null && generator=(-G Ninja)
    rpath=OFF
    [ "$prefix" != /usr ] && rpath=ON
    run cmake -S "$src" -B "$build_dir" "${generator[@]}" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$prefix" -DMEADOW_BUILD_TESTS=OFF -DMEADOW_BUILD_GALLERY=OFF \
        -DMEADOW_INSTALL_PAM=OFF -DMEADOW_INSTALL_RPATH="$rpath"
    run cmake --build "$build_dir" --parallel
fi
if [ "$dry_run" = 0 ] || [ -f "$build_dir/CMakeCache.txt" ]; then
    [ -f "$build_dir/CMakeCache.txt" ] || die "$build_dir is not a CMake build directory"
    built_prefix=$(cache_var "$build_dir" CMAKE_INSTALL_PREFIX)
    if [ "$built_prefix" != "$prefix" ] && [ "$dry_run" = 0 ]; then
        die "$build_dir was configured for prefix $built_prefix, not $prefix (paths are built in); rebuild without --no-build" 2
    fi
fi

# 3. Root for a live install: re-run just the install step through sudo.
if [ "$mode" != staged ] && [ "$dry_run" = 0 ] && [ "$(id -u)" != 0 ] && [ ! -w "${dest:-/}${prefix}" ]; then
    say "Installing needs root; re-running the install step with sudo."
    exec sudo "$0" "${orig_args[@]}" --no-build --skip-deps --build-dir "$build_dir"
fi

# 4. Stage the CMake install (component "Unspecified": everything but the
# PAM service) into a temporary directory and check it stays in the prefix.
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
if [ -f "$build_dir/CMakeCache.txt" ] && { [ "$dry_run" = 0 ] || [ -f "$build_dir/build.ninja" ] || [ -f "$build_dir/Makefile" ]; }; then
    say "+ DESTDIR=<staging> cmake --install $build_dir --component Unspecified"
    if ! DESTDIR=$stage cmake --install "$build_dir" --component Unspecified >"$stage.log" 2>&1; then
        cat "$stage.log" >&2; rm -f "$stage.log"
        [ "$dry_run" = 1 ] && { say "(the build is not complete yet; files are listed after building)"; exit 0; }
        die "cmake --install failed"
    fi
    rm -f "$stage.log"
else
    say "(files are listed once $build_dir is built)"
    system_steps
    say "Dry run: nothing was changed."
    exit 0
fi
outside=$(cd "$stage" && find . -mindepth 1 \( -type f -o -type l \) | sed 's|^\.||' | grep -v "^$prefix/" || true)
[ -z "$outside" ] || die "the build installs files outside $prefix, refusing: $(echo "$outside" | head -3 | tr '\n' ' ')"
# The manifest is whitespace-separated.
spaced=$(cd "$stage" && find . -name '* *' | head -1)
[ -z "$spaced" ] || die "file names with spaces are not supported by the manifest: ${spaced#.}"

manifest="$prefix/$MANIFEST_REL"
old_manifest=$(on_target "$manifest")
declare -A previous=()
if [ -f "$old_manifest" ]; then
    say "Updating the Meadow already installed under $prefix."
    while read -r kind a b _; do
        case $kind in file) previous[$b]=$a ;; link) previous[$a]=link ;; esac
    done <"$old_manifest"
fi

new_manifest=$(mktemp)
{
    echo "# Meadow install manifest (install.sh); read by uninstall.sh"
    echo "option prefix=$prefix"
    echo "option pam=$((1 - no_pam))"
    echo "option greeter=$greeter"
    echo "option linked=$([ "$prefix" != /usr ] && echo 1 || echo 0)"
} >"$new_manifest"
# Directories created by an earlier run stay recorded.
[ -f "$old_manifest" ] && grep '^dir ' "$old_manifest" >>"$new_manifest" || true
[ -f "$old_manifest" ] && grep '^backup ' "$old_manifest" >>"$new_manifest" || true

# Checksums of every staged file in one pass.
declare -A staged_sha=()
while read -r h f; do staged_sha[${f#.}]=$h; done < <(cd "$stage" && find . -mindepth 1 -type f -print0 | xargs -0 -r sha256sum)

count=0
while IFS= read -r d; do
    abs=${d#.}
    t=$(on_target "$abs")
    if [ ! -d "$t" ]; then
        if [ "$dry_run" = 1 ]; then say "would create $abs/"; else mkdir -p -m 755 "$t"; fi
        grep -qx "dir $abs" "$new_manifest" || echo "dir $abs" >>"$new_manifest"
    fi
done < <(cd "$stage" && find . -mindepth 1 -type d | sort)

while IFS= read -r f; do
    abs=${f#.}
    t=$(on_target "$abs")
    s="$stage$abs"
    if { [ -e "$t" ] || [ -L "$t" ]; } && [ -z "${previous[$abs]:-}" ] && [ ! -e "$t.meadow-bak" ]; then
        if [ "$dry_run" = 1 ]; then say "would back up $abs -> $abs.meadow-bak"; else cp -P --preserve=all "$t" "$t.meadow-bak"; fi
        echo "backup $abs" >>"$new_manifest"
    fi
    if [ -L "$s" ]; then
        echo "link $abs $(readlink "$s")" >>"$new_manifest"
        [ "$dry_run" = 1 ] || { rm -f "$t"; ln -s "$(readlink "$s")" "$t"; }
    else
        echo "file ${staged_sha[$abs]} $abs" >>"$new_manifest"
        [ "$dry_run" = 1 ] || { rm -f "$t"; cp --preserve=mode,timestamps "$s" "$t"; }
    fi
    unset "previous[$abs]"
    count=$((count + 1))
done < <(cd "$stage" && find . -mindepth 1 \( -type f -o -type l \) | sort)

# Files a previous version installed that this one no longer has.
for abs in "${!previous[@]}"; do
    [ "$abs" = "$manifest" ] && continue
    t=$(on_target "$abs")
    if [ "${previous[$abs]}" = link ] || { [ -f "$t" ] && [ "$(sha "$t")" = "${previous[$abs]}" ]; }; then
        if [ "$dry_run" = 1 ]; then say "would remove stale $abs"; else rm -f "$t"; fi
    fi
done

if [ "$dry_run" = 1 ]; then
    say "would install $count files under $prefix and write $manifest"
else
    install -D -m 644 "$new_manifest" "$(on_target "$manifest")"
    say "Installed $count files under $prefix (manifest: $manifest)."
    if [ "$mode" = live ] && command -v restorecon >/dev/null 2>&1 && [ "$(id -u)" = 0 ]; then
        restorecon -R "$prefix" 2>/dev/null || true
    fi
fi
rm -f "$new_manifest"

system_steps
if [ "$no_pam" = 1 ]; then
    say "--no-pam: no lock screen PAM service; Meadow will not lock the screen (meadow-setup enable-lock adds it later)."
fi
if [ "$greeter" = 0 ]; then
    say "The login screen is unchanged. To use the Meadow Welcome screen:  sudo $setup enable-greeter"
fi
[ "$dry_run" = 1 ] && say "Dry run: nothing was changed."
say "Done. Choose \"Meadow\" in the session menu of your display manager."
