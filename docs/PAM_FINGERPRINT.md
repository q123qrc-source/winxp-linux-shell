# Fingerprint login and PAM

Fingerprint login works on the Welcome screen (SDDM) and the Meadow lock
screen. It uses fprintd, libfprint and `pam_fprintd`.

## Ground rules

- Installing or removing Meadow packages **never** changes PAM, authselect or
  the display manager.
- Only `sudo meadow-setup enable-fingerprint` changes PAM, and
  `sudo meadow-setup disable-fingerprint` reverts exactly what it did.
- On Fedora, `system-auth` and `password-auth` belong to **authselect**, so
  Meadow never edits them. It runs `authselect enable-feature
  with-fingerprint`, and turns that feature off again only if Meadow was the
  one that turned it on.
- Every file Meadow changes is backed up first as `<file>.meadow-bak`. An
  existing backup is never overwritten, because the oldest one is the pristine
  copy. Every change is marked, and every action is recorded in
  `/var/lib/meadow/setup-state.json`.
- Run `meadow-setup --dry-run enable-fingerprint` first. It prints every change
  as a diff and changes nothing.
- Test PAM changes **only in a VM** first (see TESTING.md).

## What `enable-fingerprint` does

1. **Distribution profile.** On Fedora it shows `authselect current` and runs
   `authselect enable-feature with-fingerprint`. On Debian/Ubuntu it runs
   `pam-auth-update --enable fprintd`. Arch has no profile tool, so step 2
   does the work there.
2. **`/etc/pam.d/sddm`.** Meadow follows the service's `include`,
   `substack` and `@include` lines. If `pam_fprintd` is **already reachable**
   (for example Debian with the fprintd profile), the file is left alone.
   Otherwise it inserts this marked block directly before the password stack
   (the first auth `include`/`substack`, `@include common-auth`, or
   `pam_unix.so` line):

   ```
   # >>> meadow fingerprint >>>
   # Added by `meadow-setup enable-fingerprint`; removed by `meadow-setup disable-fingerprint`.
   # Fingerprint first; if it fails or times out, the password stack below runs.
   auth  [success=done default=ignore]  pam_fprintd.so max-tries=3 timeout=15
   # <<< meadow fingerprint <<<
   ```

   This is needed on **Fedora**: `with-fingerprint` adds `pam_fprintd` to
   `system-auth` only, and `/etc/pam.d/sddm` uses `password-auth`.
   `[success=done default=ignore]` means a recognized finger logs in, and
   anything else (no reader, no enrolled finger, three misses, 15 s timeout)
   falls through to the password.
3. **`/etc/pam.d/meadow-lock`** (the lock screen service). It is generated from
   the sddm service: the same auth and account stack (on Fedora,
   `substack password-auth`) without greeter-only modules (SELinux permit,
   keyrings), with the fingerprint line first unless that stack already
   reaches `pam_fprintd`. A `meadow-lock` file that isn't Meadow's is backed up
   and restored on disable. The Fedora result is in
   `/usr/share/meadow/pam.d/meadow-lock`.
4. **Optional: `--allow-greeter-query`.** This installs
   `/etc/polkit-1/rules.d/50-meadow.rules` (see below).
5. Relabels the changed files for SELinux (`restorecon`), offers a
   `pamtester` check, and prints the recovery steps.

`disable-fingerprint` removes the marked block, which keeps any edits made to
the file since, and deletes `meadow-lock` (or restores the backup). It removes
the polkit rule and turns the authselect / pam-auth-update profile off only if
Meadow turned it on. `meadow-setup status` shows the current state.

## Enrolling a finger

```sh
fprintd-enroll            # as the user; follow the prompts
fprintd-verify            # check it
```

## How the Welcome screen uses it

SDDM runs PAM only inside `sddm.login()`. So the greeter starts a fingerprint
reading by logging in with an **empty password**: `pam_fprintd` runs first,
and if it fails, `pam_unix` rejects the empty password and SDDM reports
`loginFailed`.

| `theme.conf` `fingerprint=` | Reading starts automatically when a user is selected… |
|---|---|
| `auto` (default) | …if fprintd says the user has enrolled fingers |
| `always` | …whenever a fingerprint reader is present |
| `off` | never |

In every mode, **pressing Enter with an empty password field** starts a
reading when a reader is present.

During a reading the selected tile shows "Touch the fingerprint reader, or type
your password", followed by the reader's own messages ("Swipe your finger…",
"Failed to match fingerprint"). These come from PAM and are shown, never
logged.

**Typing a password during a reading.** SDDM's daemon silently ignores a
second `login()` while one is running. So the greeter *queues* the typed
password and submits it as soon as the reading ends (match → logged in
already; no match or timeout → the password is checked). The tile says so
while it waits.

**Who has enrolled fingers?** The greeter asks fprintd read-only
(`ListEnrolledFingers`). For *another* user, fprintd requires polkit's
`net.reactivated.fprint.device.setusername`, which the unprivileged `sddm`
user does not have by default. The answer is then "unknown", and `auto` mode
does not start readings on its own. You have two choices:

- `fingerprint=always` in `theme.conf.user`, when every user has a finger
  enrolled; or
- `meadow-setup enable-fingerprint --allow-greeter-query`, which installs:

  ```js
  polkit.addRule(function(action, subject) {
      if ((action.id == "net.reactivated.fprint.device.setusername" ||
           action.id == "net.reactivated.fprint.device.verify") &&
          subject.user == "sddm" && subject.local && subject.active) {
          return polkit.Result.YES;
      }
  });
  ```

  This lets the local, active greeter list and verify any user's
  fingerprints. It cannot enroll or delete them. A user's fingerprint data
  never leaves fprintd; only "enrolled or not" and "matched or not" come back.

### Things to know

- **Account lockout.** If `pam_faillock` is enabled (authselect `with-faillock`),
  a fingerprint reading that ends without a match counts as one failed login,
  because `pam_unix` then sees the empty password. Prefer `always`/`auto` only
  where that is acceptable, or raise faillock's `deny`.
- The reader times out after 15 s per reading (`timeout=15`), and three
  unrecognized touches end it (`max-tries=3`).

## Testing with pamtester

```sh
sudo dnf install pamtester
pamtester sddm "$USER" authenticate
pamtester meadow-lock "$USER" authenticate
```

## Other distributions

- **Debian/Ubuntu:** `pam-auth-update --enable fprintd` manages `common-auth`.
  SDDM includes it, so `/etc/pam.d/sddm` usually needs no change.
- **Arch:** `/etc/pam.d/sddm` → `system-login` → `system-auth`, without
  fprintd. meadow-setup adds the marked block to `/etc/pam.d/sddm`.

## Recovery: I can't log in

You will not need this if you only installed packages. It applies only after
`meadow-setup enable-fingerprint` or `enable-greeter`.

1. **Get a text console.** Press `Ctrl+Alt+F3` (or F2–F6) at the login screen.
   If the graphical login is broken, boot with `systemd.unit=multi-user.target`
   added to the kernel command line (press `e` in GRUB).
2. **Log in** on the console with your user name and **password**. Console
   login uses `/etc/pam.d/login`, which Meadow never changes.
3. **Undo Meadow's changes automatically:**
   ```sh
   sudo meadow-setup disable-fingerprint
   sudo meadow-setup disable-greeter
   ```
4. **Or undo them by hand:**
   ```sh
   # put the SDDM PAM file back the way it was before Meadow
   sudo cp -a /etc/pam.d/sddm.meadow-bak /etc/pam.d/sddm
   sudo rm -f /etc/pam.d/meadow-lock /etc/polkit-1/rules.d/50-meadow.rules
   # Fedora: turn off authselect's fingerprint feature
   sudo authselect disable-feature with-fingerprint
   sudo authselect check
   # go back to the previous display manager (example: GDM)
   sudo rm -f /etc/sddm.conf.d/10-meadow.conf
   sudo systemctl enable --force gdm.service
   sudo rm -f /var/lib/meadow/setup-state.json
   ```
5. **If authselect itself is in a bad state:** run `sudo authselect select
   <profile> --force` with the profile shown by `authselect current`, or
   restore one of authselect's own backups: `sudo authselect backup-list`,
   then `sudo authselect backup-restore <name>`.
6. **SELinux:** if a restored file gives AVC denials, run
   `sudo restorecon -Rv /etc/pam.d /etc/sddm.conf.d /etc/polkit-1/rules.d`.
7. Reboot: `sudo systemctl reboot`.

If you can't log in on a console either (very unlikely, since Meadow never
touches `login`), boot a Fedora live USB, mount the root filesystem, and
restore `/etc/pam.d/sddm.meadow-bak` the same way.
