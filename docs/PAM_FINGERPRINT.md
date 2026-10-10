# Fingerprint login and PAM

> Status: phase 1 outline. The **recovery** section is complete now because it
> must exist before any PAM change is ever made. The rest is filled in during
> phases 4 and 11.

## Ground rules

- Installing or removing Meadow packages **never** changes PAM, authselect or
  the display manager.
- Only `sudo meadow-setup enable-fingerprint` changes PAM, and
  `disable-fingerprint` reverts it.
- On Fedora, `system-auth` and `password-auth` belong to **authselect**. Meadow
  never edits them; it only runs `authselect enable-feature with-fingerprint`.
- `/etc/pam.d/sddm` is not managed by authselect. Meadow changes it only if
  fingerprint auth isn't already reachable through its substacks. The change is
  a single marked block:

  ```
  # >>> meadow fingerprint >>>
  auth  [success=done default=ignore]  pam_fprintd.so max-tries=3 timeout=15
  # <<< meadow fingerprint <<<
  ```

  It is inserted before the password auth line, and the original file is
  backed up first as `/etc/pam.d/sddm.meadow-bak`.
- `/etc/pam.d/meadow-lock` (the lock screen service) includes the same
  substack that `/etc/pam.d/sddm` uses.
- Test PAM changes **only in a VM** (see TESTING.md).

## Enrolling a finger (phase 4)

```sh
fprintd-enroll            # as the user, follow the prompts
fprintd-verify            # check it
```

## Greeter behavior (phase 4)

SDDM starts PAM only when the greeter calls `sddm.login()`. When you select a
user who has enrolled fingers (as reported by the read-only
`meadow-fprint-query` helper, or forced with `fingerprintAutoStart` in
`theme.conf`), the greeter calls `sddm.login(user, "", session)` and shows
"Touch the fingerprint reader, or type your password". If you type a password
instead, the fingerprint attempt times out (`timeout=15`) and falls through to
`pam_unix`. Alternative: press Enter on an empty password field to start a
fingerprint attempt.

## Testing with pamtester (phase 11)

```sh
sudo dnf install pamtester
pamtester sddm "$USER" authenticate
pamtester meadow-lock "$USER" authenticate
```

## Other distributions (phase 10)

- **Debian/Ubuntu:** `pam-auth-update --enable fprintd` manages `common-auth`.
  Do not hand-edit it.
- **Arch:** add `pam_fprintd.so` to `/etc/pam.d/system-local-login`, or rely on
  the `system-login` include. Back up first.

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
   # restore the SDDM PAM file Meadow backed up
   sudo cp -a /etc/pam.d/sddm.meadow-bak /etc/pam.d/sddm
   sudo rm -f /etc/pam.d/meadow-lock
   # Fedora: turn off authselect's fingerprint feature
   sudo authselect disable-feature with-fingerprint
   sudo authselect check
   # go back to the previous display manager (example: GDM)
   sudo rm -f /etc/sddm.conf.d/10-meadow.conf
   sudo systemctl enable --force gdm.service
   ```
5. **If authselect itself is in a bad state:** `sudo authselect select <profile>
   --force` with the profile shown by `authselect current`, or restore one of
   authselect's own backups: `sudo authselect backup-list`, then
   `sudo authselect backup-restore <name>`.
6. **SELinux:** if a restored file gives AVC denials, run
   `sudo restorecon -Rv /etc/pam.d /etc/sddm.conf.d`.
7. Reboot: `sudo systemctl reboot`.

If you can't log in on a console either (very unlikely, since Meadow never
touches `login`), boot a Fedora live USB, mount the root filesystem, and
restore `/etc/pam.d/sddm.meadow-bak` the same way.
