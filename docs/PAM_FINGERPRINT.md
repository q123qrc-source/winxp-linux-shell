# Fingerprint login and PAM

Fingerprint login works on the Welcome screen (SDDM) and the Meadow lock
screen. It uses fprintd, libfprint and `pam_fprintd`.

## Ground rules

- Installing or removing Meadow packages **never** changes how anyone logs
  in: no PAM file of an existing service, no authselect profile and no display
  manager setting is touched. The one PAM file the package ships is
  `/etc/pam.d/meadow-lock` (see below). Only the Meadow lock screen reads it.
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
3. **`/etc/pam.d/meadow-lock`** (the lock screen service).
   - **The packaged file** (marker `# meadow-lock: shipped by Meadow (password
     only)`) gets the same marked fingerprint block at the top of its auth
     stack, after a `.meadow-bak` backup. If its stack already reaches
     `pam_fprintd` (for example through authselect's `with-fingerprint`),
     it is left alone.
   - **No file, or a file that isn't Meadow's** (another distribution, or the
     package was not installed): a service is generated from the sddm
     service. It has the same auth and account stack (on Fedora,
     `substack password-auth`) without greeter-only modules (SELinux permit,
     keyrings), with the fingerprint line first unless that stack already
     reaches `pam_fprintd`. A foreign file is backed up and restored on
     disable.
4. **Optional: `--allow-greeter-query`.** This installs
   `/etc/polkit-1/rules.d/50-meadow.rules` (see below).
5. Relabels the changed files for SELinux (`restorecon`), offers a
   `pamtester` check, and prints the recovery steps.

`disable-fingerprint` removes the marked blocks, which keeps any edits made to
the files since. The packaged `meadow-lock` goes back to password-only; a
generated one is deleted (or the foreign file it replaced is restored). It removes
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

## The lock screen and `/etc/pam.d/meadow-lock`

The package installs this file as `%config(noreplace)`, so an edited copy
survives updates:

```
#%PAM-1.0
# meadow-lock: shipped by Meadow (password only)
auth        substack      password-auth
auth        include       postlogin
account     required      pam_nologin.so
account     include       password-auth
```

It is a **new service name**. `login`, `sddm`, `gdm-password`, `sudo` and
every other service are unchanged, so installing it cannot lock anyone out of
logging in. A broken `meadow-lock` can at worst keep the lock screen from
unlocking; see the recovery steps below. A copy of the pristine file is kept
in `/usr/share/meadow/pam.d/meadow-lock`.

**How PAM runs.** meadow-shell never loads PAM itself. Each unlock attempt
starts `/usr/libexec/meadow/meadow-auth-helper`, a small process that is
**not setuid** and runs as the locked user. pam_unix checks the user's own
password through `unix_chkpwd`, as every screen locker does. The helper:

- authenticates only the user who started it (`getuid()`); the shell cannot
  ask for another account;
- runs `pam_authenticate`, `pam_acct_mgmt` and `pam_setcred(PAM_REFRESH_CRED)`;
- forwards prompts and messages to the shell over stdin/stdout, as
  length-framed messages: `PROMPT_ECHO_OFF`, `PROMPT_ECHO_ON`, `INFO`,
  `ERROR`, `RESULT` from the helper; `RESPONSE` and `CANCEL` from the shell;
- wipes the password (`explicit_bzero`), disables core dumps
  (`PR_SET_DUMPABLE 0`) and logs nothing it was told or typed. The shell logs
  only "password unlock attempt" and "authentication succeeded/failed" under
  `meadow.shell.auth` / `meadow.shell.lock`.

A PAM module that crashes or hangs takes down the helper, not the shell, so
the session stays locked either way.

**Fingerprint on the lock screen.** This works exactly like the Welcome
screen, using the same QML (`Meadow.Welcome`). With fingers enrolled, a
reading starts as soon as the lock screen appears (an empty-password attempt
reaches `pam_fprintd` first). A password typed during a reading is queued.
When the reading ends without a match, the helper reaches pam_unix's prompt;
a fingerprint attempt then cancels instead of answering, and the queued
password is checked next. A second prompt in one conversation (for example
"your password has expired") cannot be answered from a lock screen and
fails the attempt.

**What locks the session:** Win+L (`meadow-shellctl lock`), `loginctl
lock-session` (logind's `Lock` signal), suspend when `[ScreenSaver]
lockOnSuspend=true` (a logind delay inhibitor locks before the system
sleeps), and the end of a password-protected screen saver. Locking uses the
compositor's `ext-session-lock-v1`. If meadow-shell dies while locked, labwc
keeps the screen locked and blank. The restarted shell finds
`$XDG_RUNTIME_DIR/meadow/locked` and locks again.

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
   sudo rm -f /etc/polkit-1/rules.d/50-meadow.rules
   # the lock screen service back to the packaged password-only file
   sudo cp /usr/share/meadow/pam.d/meadow-lock /etc/pam.d/meadow-lock
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

### The lock screen won't unlock

1. Press `Ctrl+Alt+F3` and log in on the text console as the same user.
2. Unlock the graphical session from there. Your open windows are kept.
   meadow-shell honours logind's Unlock, which logind accepts only from the
   session's own user or an administrator:
   ```sh
   loginctl list-sessions         # find the graphical session's id
   loginctl unlock-session <id>
   ```
   If meadow-shell is hung and doesn't react, `pkill -x meadow-shell`; the
   session restarts it and it locks again. As a last resort, end the graphical
   session with `loginctl terminate-session <id>`.
3. Put the lock service back:
   ```sh
   sudo cp -a /etc/pam.d/meadow-lock.meadow-bak /etc/pam.d/meadow-lock   # Meadow's backup, if any
   # or the pristine packaged copy:
   sudo cp /usr/share/meadow/pam.d/meadow-lock /etc/pam.d/meadow-lock
   sudo restorecon -v /etc/pam.d/meadow-lock
   pamtester meadow-lock "$USER" authenticate
   ```
4. `Ctrl+Alt+F1` (or F2) returns to the graphical session.

If you can't log in on a console either (very unlikely, since Meadow never
touches `login`), boot a Fedora live USB, mount the root filesystem, and
restore `/etc/pam.d/sddm.meadow-bak` the same way.
