#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Procedurally synthesize Meadow's original sound scheme (phase 9).

Each event in theme.json "sounds" is generated from additive synthesis and
envelopes (no samples), written as Ogg Vorbis to toolkit-themes/sounds/meadow/stereo/.
"""
import sys

if __name__ == "__main__":
    print("gen-sounds.py: not implemented until phase 9", file=sys.stderr)
    sys.exit(3)
