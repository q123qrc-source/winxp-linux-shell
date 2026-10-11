#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthesize Meadow's original sound scheme (phase 9).

Every sound is built here from sine partials, envelopes and a seeded noise
source: no recordings or samples are involved. The PCM is deterministic, so
its SHA-256 is recorded in pcm.sha256 next to the theme; the sounds_sync
ctest regenerates the PCM (no encoder needed) and compares hashes. Encoding
to Ogg Vorbis uses oggenc and is needed only when (re)writing the files.

Files use freedesktop sound-naming (system-bootup, desktop-login, ...), so
libcanberra-based applications find them in the "meadow" sound theme too.

  gen-sounds.py [--output DIR]     synthesize + encode DIR/stereo/*.oga and
                                   DIR/pcm.sha256 (default: the source tree)
  gen-sounds.py --check [DIR]      compare PCM hashes with DIR/pcm.sha256
  gen-sounds.py --wav DIR          write the uncompressed PCM as .wav (listening)
"""
import argparse
import array
import hashlib
import math
import shutil
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = ROOT / "toolkit-themes" / "sounds" / "meadow"
RATE = 44100
PEAK = 0.7  # about -3 dBFS


def hz(note):
    """Note name like "C4", "F#5" -> frequency (A4 = 440 Hz)."""
    names = {"C": -9, "D": -7, "E": -5, "F": -4, "G": -2, "A": 0, "B": 2}
    n, rest = note[0], note[1:]
    semis = names[n]
    if rest.startswith("#"):
        semis, rest = semis + 1, rest[1:]
    elif rest.startswith("b"):
        semis, rest = semis - 1, rest[1:]
    return 440.0 * 2 ** ((semis + (int(rest) - 4) * 12) / 12)


class Track:
    """A stereo buffer that voices are mixed into."""

    def __init__(self, seconds):
        self.n = int(seconds * RATE)
        self.left = [0.0] * self.n
        self.right = [0.0] * self.n

    def voice(self, freq, start, dur, amp=0.3, partials=((1, 1.0),), attack=0.005, decay=None,
              release=0.05, pan=0.0, detune=0.0, vibrato=0.0):
        """One note: summed partials (ratio, gain) under an attack / exponential
        decay / release envelope. decay is the time to fall to 1/e."""
        i0 = int(start * RATE)
        length = int(dur * RATE)
        gl, gr = amp * (1 - max(pan, 0)), amp * (1 + min(pan, 0))
        two_pi = 2 * math.pi
        rel_start = max(0, length - int(release * RATE))
        for k in range(length):
            i = i0 + k
            if i >= self.n:
                break
            t = k / RATE
            env = min(1.0, t / attack) if attack > 0 else 1.0
            if decay:
                env *= math.exp(-t / decay)
            if k >= rel_start and length > rel_start:
                env *= 1 - (k - rel_start) / (length - rel_start)
            if env < 1e-5:
                continue
            f = freq * (1 + vibrato * math.sin(two_pi * 5.2 * t))
            s = 0.0
            for ratio, g in partials:
                s += g * math.sin(two_pi * f * ratio * t)
                if detune:
                    s += g * 0.5 * math.sin(two_pi * f * ratio * (1 + detune) * t)
            s *= env
            self.left[i] += s * gl
            self.right[i] += s * gr

    def noise(self, start, dur, amp=0.2, seed=1, smooth=0.6, attack=0.002, decay=0.1, crackle=0.0):
        """Seeded, low-passed noise (one-pole filter); crackle adds random bursts."""
        i0 = int(start * RATE)
        state, y = seed, 0.0
        burst = 1.0
        for k in range(int(dur * RATE)):
            i = i0 + k
            if i >= self.n:
                break
            state = (state * 1103515245 + 12345) & 0x7FFFFFFF
            x = state / 0x3FFFFFFF - 1.0
            y = smooth * y + (1 - smooth) * x
            t = k / RATE
            env = min(1.0, t / attack) * math.exp(-t / decay)
            if crackle and k % 441 == 0:
                state = (state * 1103515245 + 12345) & 0x7FFFFFFF
                burst = 0.25 + (state / 0x7FFFFFFF) * crackle
            v = y * env * amp * burst
            self.left[i] += v
            self.right[i] += v * 0.9

    def pcm(self):
        peak = max(1e-9, max(max(abs(x) for x in self.left), max(abs(x) for x in self.right)))
        g = PEAK / peak
        data = array.array("h")
        for a, b in zip(self.left, self.right):
            data.append(int(round(max(-1.0, min(1.0, a * g)) * 32767)))
            data.append(int(round(max(-1.0, min(1.0, b * g)) * 32767)))
        if sys.byteorder == "big":
            data.byteswap()
        return data.tobytes()


# Timbres: (ratio, gain) partial lists.
CHIME = ((1, 1.0), (2.0, 0.35), (3.01, 0.12), (4.2, 0.05))
BELL = ((1, 1.0), (2.76, 0.4), (5.4, 0.18), (8.93, 0.06))
PAD = ((1, 1.0), (2, 0.25), (3, 0.08))
SOFT = ((1, 1.0), (2, 0.15))


def pad(tr, notes, start, dur, amp=0.12, attack=0.6, release=0.8):
    for k, n in enumerate(notes):
        tr.voice(hz(n), start, dur, amp, PAD, attack=attack, release=release, pan=(-0.4 if k % 2 else 0.4),
                 detune=0.003)


def arpeggio(tr, notes, start, step, amp=0.22, timbre=CHIME, decay=0.7, length=1.6):
    for k, n in enumerate(notes):
        tr.voice(hz(n), start + k * step, length, amp, timbre, attack=0.004, decay=decay, release=0.2,
                 pan=-0.5 + k / max(1, len(notes) - 1))


def system_bootup():
    tr = Track(4.4)
    pad(tr, ["C3", "G3", "E4", "D5"], 0.0, 4.2, attack=1.2, release=1.6)
    arpeggio(tr, ["C5", "E5", "G5", "B5", "D6"], 0.55, 0.32, decay=1.1, length=2.6)
    tr.voice(hz("C6"), 2.3, 2.0, 0.12, CHIME, attack=0.01, decay=1.2, release=0.6)
    tr.voice(hz("G6"), 2.35, 2.0, 0.06, CHIME, attack=0.01, decay=1.0, release=0.6, pan=0.4)
    return tr


def desktop_login():
    tr = Track(2.4)
    pad(tr, ["G3", "D4", "B4"], 0.0, 2.3, amp=0.09, attack=0.4, release=1.0)
    arpeggio(tr, ["G4", "B4", "D5", "G5"], 0.1, 0.16, decay=0.8, length=2.0)
    return tr


def desktop_logout():
    tr = Track(2.2)
    pad(tr, ["G3", "D4"], 0.0, 2.1, amp=0.08, attack=0.3, release=1.0)
    arpeggio(tr, ["G5", "D5", "B4", "G4"], 0.05, 0.18, amp=0.2, decay=0.8, length=1.8)
    return tr


def system_shutdown():
    tr = Track(3.2)
    pad(tr, ["C3", "G3", "E4"], 0.0, 3.0, amp=0.1, attack=0.3, release=1.8)
    arpeggio(tr, ["D6", "B5", "G5", "E5", "C5"], 0.1, 0.26, amp=0.2, decay=0.9, length=2.2)
    return tr


def dialog_error():
    tr = Track(0.9)
    for n, p in (("A3", -0.2), ("C4", 0.2), ("E4", 0.0)):
        tr.voice(hz(n), 0.0, 0.85, 0.25, BELL, attack=0.003, decay=0.22, release=0.2, pan=p)
    tr.voice(hz("A2"), 0.0, 0.5, 0.25, SOFT, attack=0.003, decay=0.12, release=0.1)
    return tr


def dialog_warning():
    tr = Track(0.8)
    tr.voice(hz("E5"), 0.0, 0.75, 0.3, BELL, attack=0.003, decay=0.25, release=0.2)
    tr.voice(hz("B4"), 0.0, 0.6, 0.12, SOFT, attack=0.003, decay=0.2, release=0.2)
    return tr


def dialog_information():
    tr = Track(0.9)
    tr.voice(hz("E6"), 0.0, 0.5, 0.22, CHIME, attack=0.004, decay=0.25, release=0.1, pan=-0.2)
    tr.voice(hz("B5"), 0.13, 0.7, 0.24, CHIME, attack=0.004, decay=0.3, release=0.2, pan=0.2)
    return tr


def dialog_question():
    tr = Track(0.9)
    tr.voice(hz("G5"), 0.0, 0.45, 0.22, SOFT, attack=0.02, decay=0.3, release=0.1, pan=-0.2)
    tr.voice(hz("D6"), 0.16, 0.65, 0.22, SOFT, attack=0.02, decay=0.35, release=0.2, pan=0.2, vibrato=0.004)
    return tr


def message_new_instant():
    tr = Track(0.55)
    tr.noise(0.0, 0.05, 0.12, seed=7, smooth=0.3, decay=0.012)
    tr.voice(hz("A5"), 0.01, 0.4, 0.26, CHIME, attack=0.002, decay=0.12, release=0.1)
    tr.voice(hz("E6"), 0.07, 0.4, 0.16, CHIME, attack=0.002, decay=0.12, release=0.1, pan=0.3)
    return tr


def device_added():
    tr = Track(0.8)
    tr.voice(hz("E5"), 0.0, 0.4, 0.24, CHIME, attack=0.003, decay=0.15, release=0.1, pan=-0.3)
    tr.voice(hz("B5"), 0.14, 0.6, 0.24, CHIME, attack=0.003, decay=0.22, release=0.2, pan=0.3)
    return tr


def device_removed():
    tr = Track(0.8)
    tr.voice(hz("B5"), 0.0, 0.4, 0.24, CHIME, attack=0.003, decay=0.15, release=0.1, pan=0.3)
    tr.voice(hz("E5"), 0.14, 0.6, 0.24, CHIME, attack=0.003, decay=0.22, release=0.2, pan=-0.3)
    return tr


def bell():
    tr = Track(0.45)
    tr.voice(hz("A5"), 0.0, 0.42, 0.3, BELL, attack=0.002, decay=0.1, release=0.1)
    return tr


def trash_empty():
    tr = Track(0.9)
    tr.noise(0.0, 0.85, 0.35, seed=42, smooth=0.55, attack=0.01, decay=0.3, crackle=1.5)
    tr.noise(0.18, 0.5, 0.2, seed=99, smooth=0.75, attack=0.01, decay=0.15, crackle=1.2)
    return tr


SOUNDS = {
    "system-bootup": system_bootup, "desktop-login": desktop_login, "desktop-logout": desktop_logout,
    "system-shutdown": system_shutdown, "dialog-error": dialog_error, "dialog-warning": dialog_warning,
    "dialog-information": dialog_information, "dialog-question": dialog_question,
    "message-new-instant": message_new_instant, "device-added": device_added, "device-removed": device_removed,
    "bell": bell, "trash-empty": trash_empty,
}


def write_wav(path, pcm):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm)


def render_all():
    return {name: fn().pcm() for name, fn in SOUNDS.items()}


def manifest(pcms):
    return "".join(f"{hashlib.sha256(p).hexdigest()}  {n}.oga\n" for n, p in sorted(pcms.items()))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--output", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--check", nargs="?", const=DEFAULT_OUT, type=Path, metavar="DIR")
    ap.add_argument("--wav", type=Path, metavar="DIR")
    args = ap.parse_args()
    pcms = render_all()
    if args.check:
        want = (args.check / "pcm.sha256").read_text()
        have = manifest(pcms)
        missing = [n for n in SOUNDS if not (args.check / "stereo" / f"{n}.oga").is_file()]
        extra = sorted(p.stem for p in (args.check / "stereo").glob("*") if p.stem not in SOUNDS)
        if want != have or missing or extra:
            print(f"{args.check} is out of date (missing {missing}, extra {extra}); run tools/gen-sounds.py")
            return 1
        print(f"sounds: {len(SOUNDS)} sounds up to date")
        return 0
    if args.wav:
        args.wav.mkdir(parents=True, exist_ok=True)
        for n, p in pcms.items():
            write_wav(args.wav / f"{n}.wav", p)
        return 0
    oggenc = shutil.which("oggenc")
    if not oggenc:
        print("gen-sounds.py: oggenc (vorbis-tools) is needed to encode the sounds", file=sys.stderr)
        return 3
    stereo = args.output / "stereo"
    shutil.rmtree(stereo, ignore_errors=True)
    stereo.mkdir(parents=True)
    with tempfile.TemporaryDirectory() as tmp:
        for n, p in pcms.items():
            wav = Path(tmp) / f"{n}.wav"
            write_wav(wav, p)
            subprocess.run([oggenc, "-Q", "-q", "5", "--serial", "1", "-t", n.replace("-", " ").title(),
                            "-a", "Meadow", "-c", "LICENSE=CC-BY-SA-4.0", "-o", str(stereo / f"{n}.oga"), str(wav)],
                           check=True)
    (args.output / "pcm.sha256").write_text(manifest(pcms))
    print(f"wrote {len(SOUNDS)} sounds to {stereo}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
