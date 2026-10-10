#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Generic installer for non-RPM systems (phase 10). Will support --dry-run,
# --prefix, DESTDIR and --no-pam; on Fedora (ID=fedora in /etc/os-release) it
# delegates every system change to meadow-setup.
set -euo pipefail

case "${1:-}" in
    -h|--help)
        echo "Usage: install.sh [--dry-run] [--prefix DIR] [--no-pam]"
        echo "Not implemented until phase 10. On Fedora, use the RPM packages."
        exit 0 ;;
esac
echo "install.sh: not implemented until phase 10; nothing was changed." >&2
exit 3
