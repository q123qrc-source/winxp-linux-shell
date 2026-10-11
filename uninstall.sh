#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Reverses install.sh: asks meadow-setup to revert the system changes it made
# (greeter, lock service, prefix links), then removes the files listed in the
# install manifest. Files changed since they were installed are kept, and
# files install.sh replaced are restored from their *.meadow-bak copies.
set -euo pipefail

src=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
. "$src/tools/install-lib.sh"

usage() {
    cat <<EOF
Usage: uninstall.sh [options]

  --prefix DIR     the prefix Meadow was installed to (default /usr/local)
  --root DIR       uninstall from DIR as if it were / (as install.sh --root)
  --destdir DIR    remove a staged install (also: DESTDIR=DIR); system steps
                   are printed, not run
  --dry-run        print every step; change nothing
  -h, --help       this help
EOF
}

prefix=/usr/local
root=""
destdir=${DESTDIR:-}
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
        --dry-run) dry_run=1 ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
    shift
done
case $prefix in /*) ;; *) die "--prefix must be absolute" 2 ;; esac
prefix=$(realpath -m "$prefix")
[ -n "$root" ] && [ -n "$destdir" ] && die "use either --root or --destdir" 2
if [ -n "$root" ]; then dest=$(realpath -m "$root"); mode=root
elif [ -n "$destdir" ]; then dest=$(realpath -m "$destdir"); mode=staged
else dest=/; mode=live; fi
[ "$dest" = / ] && dest=""

manifest=$(on_target "$prefix/$MANIFEST_REL")
[ -f "$manifest" ] || die "no install manifest at ${dest}$prefix/$MANIFEST_REL; was Meadow installed there with install.sh? (packages: use the package manager)" 3

if [ "$mode" = live ] && [ "$dry_run" = 0 ] && [ "$(id -u)" != 0 ] && [ ! -w "$manifest" ]; then
    say "Uninstalling needs root; re-running with sudo."
    exec sudo "$0" "${orig_args[@]}"
fi

opt() { sed -n "s/^option $1=//p" "$manifest" | tail -1; }
say "Meadow uninstall: prefix $prefix${dest:+ in $dest}"
[ "$dry_run" = 1 ] && say "Dry run: nothing will be changed."

# 1. System changes, newest first, while meadow-setup is still installed.
setup="$prefix/bin/meadow-setup"
setup_cmds=()
[ "$(opt greeter)" = 1 ] && setup_cmds+=(disable-greeter)
[ "$(opt pam)" = 1 ] && setup_cmds+=(disable-lock)
[ "$(opt linked)" = 1 ] && setup_cmds+=(unlink-prefix)
for cmd in "${setup_cmds[@]}"; do
    case $mode in
        staged) say "run as root on the installed system:  $setup $cmd" ;;
        root)
            if [ "$dry_run" = 1 ]; then say "+ $setup --root $dest --yes $cmd"
            else
                say "+ meadow-setup --root $dest $cmd"
                python3 "$(on_target "$setup")" --root "$dest" --yes "$cmd" \
                    || die "meadow-setup $cmd failed; fix that first (e.g. meadow-setup disable-fingerprint), nothing was removed"
            fi ;;
        live)
            if [ "$dry_run" = 1 ]; then say "+ $setup --yes $cmd"
            else
                say "+ $setup --yes $cmd"
                "$setup" --yes "$cmd" \
                    || die "meadow-setup $cmd failed; fix that first (e.g. meadow-setup disable-fingerprint), nothing was removed"
            fi ;;
    esac
done

# 2. Files and links, then backups, then the directories install.sh created.
removed=0 kept=0
declare -A current_sha=()
while read -r h f; do current_sha[$f]=$h; done < <(sed -n 's/^file [0-9a-f]* //p' "$manifest" \
    | while IFS= read -r f; do t=$(on_target "$f"); [ -f "$t" ] && [ ! -L "$t" ] && printf '%s\0' "$t"; done \
    | xargs -0 -r sha256sum)
while read -r kind a b _; do
    case $kind in
        file)
            t=$(on_target "$b")
            [ -f "$t" ] && [ ! -L "$t" ] || continue
            if [ "${current_sha[$t]:-}" = "$a" ]; then
                [ "$dry_run" = 1 ] || rm -f "$t"
                removed=$((removed + 1))
            else
                warn "$b changed since it was installed; kept"
                kept=$((kept + 1))
            fi ;;
        link)
            t=$(on_target "$a")
            if [ -L "$t" ] && [ "$(readlink "$t")" = "$b" ]; then
                [ "$dry_run" = 1 ] || rm -f "$t"
                removed=$((removed + 1))
            fi ;;
    esac
done <"$manifest"

while read -r kind a _; do
    [ "$kind" = backup ] || continue
    t=$(on_target "$a")
    if [ -e "$t.meadow-bak" ] || [ -L "$t.meadow-bak" ]; then
        if [ -e "$t" ] || [ -L "$t" ]; then
            warn "$a exists again; its backup $a.meadow-bak is kept"
        elif [ "$dry_run" = 1 ]; then say "would restore $a from $a.meadow-bak"
        else mv "$t.meadow-bak" "$t"; fi
    fi
done <"$manifest"

dirs=$(sed -n 's/^dir //p' "$manifest" | sort -r)
[ "$dry_run" = 1 ] || rm -f "$manifest"
while IFS= read -r d; do
    [ -n "$d" ] || continue
    t=$(on_target "$d")
    if [ -d "$t" ] && [ -z "$(ls -A "$t")" ]; then
        [ "$dry_run" = 1 ] || rmdir "$t"
    fi
done <<<"$dirs"

if [ "$dry_run" = 1 ]; then
    say "would remove $removed files and links"
    say "Dry run: nothing was changed."
else
    say "Removed $removed files and links.$([ "$kept" -gt 0 ] && echo " Kept $kept changed files.")"
fi
