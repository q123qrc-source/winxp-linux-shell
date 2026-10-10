#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Produce meadow-<version>.tar.gz (the spec's Source0) from git HEAD.
# Version: the latest vX.Y.Z tag if HEAD is tagged, else project() in CMakeLists.txt.
set -euo pipefail

top="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
cd "$top"

version="$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)$/\1/p' CMakeLists.txt | head -n1)"
if tag="$(git describe --tags --exact-match --match 'v[0-9]*' 2>/dev/null)"; then
    version="${tag#v}"
fi
[ -n "$version" ] || { echo "make-tarball: cannot determine version" >&2; exit 1; }

outdir="${1:-$top}"
out="$outdir/meadow-$version.tar.gz"
git archive --format=tar.gz --prefix="meadow-$version/" -o "$out" HEAD
chmod 0644 "$out"
echo "$out"
