#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Asset policy guard (README.md, "Asset policy"): fail if the tree contains
# file names typical of Microsoft assets or any font/audio binary we did not
# generate ourselves. Runs in ctest and CI.
set -euo pipefail
top="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$top"

# Tracked files in a checkout; every file in an exported tarball.
list_files() {
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git ls-files --cached --others --exclude-standard
    else
        find . -type f ! -path "./build*" | sed "s|^\./||"
    fi
}

fail=0
# Names of well-known proprietary assets (case-insensitive).
banned='(bliss|luna\.msstyles|uxtheme|tahoma[a-z]*\.ttf|trebuc[a-z]*\.ttf|framd[a-z]*\.ttf|windows xp|winxp[^-]|\.wav$|\.ttf$|\.otf$|\.msstyles$|\.cur$|\.ani$)'
while IFS= read -r f; do
    echo "asset policy: disallowed file: $f" >&2
    fail=1
done < <(list_files | grep -iE "$banned" || true)

# Nothing may ever be shipped inside an assets-override directory.
while IFS= read -r f; do
    echo "asset policy: files must not be shipped in assets-override/: $f" >&2
    fail=1
done < <(list_files | grep -E '(^|/)assets-override/' || true)

[ "$fail" -eq 0 ] && echo "asset policy: OK"
exit "$fail"
