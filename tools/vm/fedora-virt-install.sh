#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Create a Fedora test VM (SELinux enforcing) with virt-install + fedora.ks.
# Phase 11 fills in the release selection, disk and USB passthrough options.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
release="${FEDORA_RELEASE:-$(rpm -E %fedora 2>/dev/null || echo 41)}"
echo "would run: virt-install --name meadow-f$release --memory 4096 --vcpus 2 --disk size=20 \\"
echo "  --location https://download.fedoraproject.org/pub/fedora/linux/releases/$release/Everything/x86_64/os/ \\"
echo "  --initrd-inject $here/fedora.ks --extra-args 'inst.ks=file:/fedora.ks'"
echo "fedora-virt-install.sh: dry outline only until phase 11" >&2
