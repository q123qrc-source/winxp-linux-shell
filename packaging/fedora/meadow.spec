# SPDX-License-Identifier: GPL-3.0-or-later
# Fedora packaging for Meadow. Primary distribution target.
#
# Policy: installing or removing these packages never changes how the system
# logs in. No scriptlet touches /etc, PAM, authselect or the display manager;
# that is done only on request by `meadow-setup` (see README-fedora.md).
# The one file under /etc is /etc/pam.d/meadow-lock, a password-only PAM
# service read only by the Meadow lock screen's helper. It is not part of the
# login stack, so it cannot change how anyone logs in.

Name:           meadow
Version:        0.1.0
Release:        1%{?dist}
Summary:        Classic Luna-style desktop: shell, Welcome screen and themes

# Code is GPL-3.0-or-later; original artwork (SVG, sounds) is CC-BY-SA-4.0.
License:        GPL-3.0-or-later AND CC-BY-SA-4.0
URL:            https://github.com/q123qrc-source/winxp-linux-shell
# Produced by tools/make-tarball.sh (git archive).
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.24
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  extra-cmake-modules
BuildRequires:  cmake(Qt6Core) >= 6.5
BuildRequires:  cmake(Qt6Gui)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  cmake(Qt6QuickControls2)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Xml)
BuildRequires:  cmake(Qt6Test)
BuildRequires:  cmake(Qt6WaylandClient)
BuildRequires:  cmake(Qt6Multimedia)
BuildRequires:  cmake(Qt6LinguistTools)
BuildRequires:  cmake(LayerShellQt)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-protocols)
# wlr-foreign-toplevel-management (taskbar window list)
BuildRequires:  pkgconfig(wlr-protocols)
BuildRequires:  polkit-qt6-1-devel
# meadow-auth-helper (lock screen)
BuildRequires:  pam-devel
# Lock-screen surfaces use QtWaylandClient's shell-integration API
# (ext-session-lock-v1), which is private: see the Requires pin below.
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  desktop-file-utils
# Icon checks, cursor theme (SVG -> XCursor) and the session's dconf
# defaults database are built from the sources.
BuildRequires:  librsvg2-tools
BuildRequires:  xcursorgen
BuildRequires:  dconf
BuildRequires:  python3
# Needed by %%ctest: the QML smoke test renders offscreen.
BuildRequires:  qt6-qtbase-gui
BuildRequires:  qt6-qtsvg
# %%ctest runs D-Bus tests on a private session bus (skipped without it).
BuildRequires:  dbus-daemon
# %%ctest checks the auth helper with pam_wrapper/pam_matrix (a private PAM
# service directory; no system PAM file is read).
BuildRequires:  pam_wrapper

Requires:       %{name}-shell%{?_isa} = %{version}-%{release}
Requires:       sddm-theme-%{name} = %{version}-%{release}
Requires:       %{name}-themes = %{version}-%{release}

%description
Meadow is a Wayland desktop that recreates the look and feel of a classic
early-2000s "Luna" desktop with entirely original artwork: a taskbar with a
green start button, a two-column Start menu, an SDDM Welcome screen, labwc
window decorations and matching GTK, Qt, icon, cursor and sound themes.

This metapackage installs all Meadow components. It does not change the
active display manager or PAM; run `meadow-setup` for that.

%package        common
Summary:        Shared library and Luna QML controls for Meadow
License:        GPL-3.0-or-later
Requires:       qt6-qtsvg

%description    common
The Meadow theme loader, sound player and the Meadow.Controls QML module,
shared by the Meadow shell and the Meadow SDDM theme.

%package        shell
Summary:        Meadow desktop shell, session files and setup tool
License:        GPL-3.0-or-later
Requires:       %{name}-common%{?_isa} = %{version}-%{release}
Requires:       %{name}-themes = %{version}-%{release}
Requires:       labwc >= 0.9
# Private QtWaylandClient API (lock surfaces): rebuild on every Qt update.
%{?_qt6_version:Requires: qt6-qtbase%{?_isa} = %{_qt6_version}}
# Lock screen (meadow-auth-helper) and the authentication agent.
Requires:       pam
Requires:       polkit
# busctl (meadow-shellctl)
Requires:       systemd
Requires:       xorg-x11-server-Xwayland
Requires:       xdg-desktop-portal-wlr
Requires:       xdg-desktop-portal-gtk
Requires:       pipewire
Requires:       wireplumber
Requires:       NetworkManager
Requires:       upower
# "Open Network Connections" in the network tray menu.
Recommends:     nm-connection-editor
# Opening documents and URLs (Start menu, Run).
Requires:       xdg-utils
# Screen capture for the Log Off / Turn Off gray fade (dark overlay without it).
Recommends:     grim
# Start menu "Printers and Faxes" and "Help and Support".
Suggests:       system-config-printer
Suggests:       yelp
Requires:       pcmanfm-qt
Requires:       kvantum
# meadow-setup
Requires:       python3
Suggests:       pamtester

%description    shell
meadow-shell draws the taskbar, Start menu, notification area, desktop and
system dialogs as layer-shell surfaces on labwc. Also contains the Meadow
Wayland session entry and the opt-in `meadow-setup` system configuration tool.

%package -n     sddm-theme-%{name}
Summary:        Meadow Welcome screen theme for SDDM
License:        GPL-3.0-or-later AND CC-BY-SA-4.0
BuildArch:      noarch
Requires:       sddm
Requires:       qt6-qtdeclarative
# SVG logo override and avatars.
Requires:       qt6-qtsvg
Requires:       %{name}-common = %{version}-%{release}
# Scheme files (theme.json), avatars and logo live in meadow-themes.
Requires:       %{name}-themes = %{version}-%{release}
Recommends:     fprintd
Recommends:     fprintd-pam
Recommends:     qt6-qtvirtualkeyboard

%description -n sddm-theme-%{name}
A Luna-style Welcome screen for SDDM with user tiles, session and keyboard
layout switchers, fingerprint support and a Turn Off Computer dialog.
Installing it does not make it the active theme; see `meadow-setup`.

%package        themes
Summary:        Meadow color schemes, toolkit, icon, cursor and sound themes
License:        GPL-3.0-or-later AND CC-BY-SA-4.0
BuildArch:      noarch
Requires:       fontconfig

%description    themes
Meadow's Luna Blue, Olive Green and Silver scheme definitions, the Meadow
logo and default user pictures, plus, for each scheme, GTK 3/4 and labwc
themes (Meadow-Luna*) and a Kvantum theme (MeadowLuna*), all generated from
the scheme definitions; the original meadow-icons, meadow-cursors and Meadow
sound themes; and font aliases mapping classic UI font names to open fonts.
The font aliases are not enabled system-wide: Meadow sessions load them.

%prep
%autosetup -p1

%build
%cmake -G Ninja \
    -DMEADOW_BUILD_TESTS=ON \
    -DMEADOW_BUILD_GALLERY=ON
%cmake_build

%install
%cmake_install
# Translations arrive in phase 7; then: %%find_lang %%{name} --with-qt

%check
# Session entries carry DesktopNames=, which desktop-file-validate rejects;
# the script checks it and validates the rest.
tools/validate-session-desktop.sh %{buildroot}%{_datadir}/wayland-sessions/meadow.desktop
desktop-file-validate %{buildroot}%{_datadir}/applications/meadow-control-panel.desktop
desktop-file-validate %{buildroot}%{_datadir}/applications/meadow-display-properties.desktop
# Screenshot comparisons depend on the exact fonts installed (CI runs them);
# the timing budget depends on the machine's load (run by hand: ctest -L perf).
%ctest --label-exclude 'screenshot|perf'

%files
# Metapackage: no files of its own besides the license and README.
%license LICENSE LICENSES/*
%doc README.md

%files common
%license LICENSE LICENSES/*
%doc README.md
%{_libdir}/libmeadow-common.so.0
%{_libdir}/libmeadow-common.so.%{version}
%dir %{_qt6_qmldir}/Meadow
%{_qt6_qmldir}/Meadow/Controls/
%{_qt6_qmldir}/Meadow/Welcome/

%files shell
%license LICENSE LICENSES/*
%doc README.md docs/ARCHITECTURE.md docs/PAM_FINGERPRINT.md docs/ALTERNATIVES.md
%doc packaging/fedora/README-fedora.md
%{_bindir}/meadow-shell
%{_bindir}/meadow-shellctl
%{_bindir}/meadow-session
%{_bindir}/meadow-shell-supervise
%{_bindir}/meadow-setup
%{_bindir}/meadow-control-panel
%dir %{_libexecdir}/meadow
%{_libexecdir}/meadow/meadow-auth-helper
# Password-only; meadow-setup enable-fingerprint adds a marked block.
%config(noreplace) %{_sysconfdir}/pam.d/meadow-lock
%{_datadir}/applications/meadow-control-panel.desktop
%{_datadir}/applications/meadow-display-properties.desktop
%dir %{_datadir}/wayland-sessions
%{_datadir}/wayland-sessions/meadow.desktop
%{_datadir}/xdg-desktop-portal/meadow-portals.conf
%dir %{_datadir}/dbus-1/interfaces
%{_datadir}/dbus-1/interfaces/org.meadow.Shell.xml
%dir %{_datadir}/%{name}
%{_datadir}/%{name}/labwc/
%{_datadir}/%{name}/shell.conf
%{_datadir}/%{name}/menus/
%{_datadir}/%{name}/desktop-directories/
%{_datadir}/%{name}/sddm.conf.d/
%{_datadir}/%{name}/pam.d/
%{_datadir}/%{name}/polkit/
# Session-scoped defaults for applications (DCONF_PROFILE, XDG_CONFIG_DIRS).
%{_datadir}/%{name}/dconf/
%{_datadir}/%{name}/xdg/

%files -n sddm-theme-%{name}
%license LICENSE LICENSES/*
%doc README.md
%{_datadir}/sddm/themes/%{name}/

%files themes
%license LICENSE LICENSES/*
%doc docs/VISUAL_SPEC.md
%dir %{_datadir}/%{name}
%{_datadir}/%{name}/themes/
%{_datadir}/%{name}/assets/
%{_datadir}/themes/Meadow-Luna/
%{_datadir}/themes/Meadow-Luna-Olive/
%{_datadir}/themes/Meadow-Luna-Silver/
%dir %{_datadir}/Kvantum
%{_datadir}/Kvantum/MeadowLuna/
%{_datadir}/Kvantum/MeadowLunaOlive/
%{_datadir}/Kvantum/MeadowLunaSilver/
%{_datadir}/icons/meadow-icons/
%{_datadir}/icons/meadow-cursors/
%dir %{_datadir}/sounds
%{_datadir}/sounds/meadow/
%{_datadir}/fontconfig/conf.avail/50-meadow.conf
%{_datadir}/%{name}/fontconfig/

%changelog
* Sun Oct 11 2026 Meadow contributors <q123qrc@gmail.com> - 0.1.0-1
- Phase 10: meadow-shell-supervise restarts the shell after a crash (with a
  recovery menu when it gives up); the shell renders in software by default
  (shell.conf [Performance]); meadow-setup enable-/disable-greeter,
  enable-/disable-lock and link-/unlink-prefix for install.sh; Arch and
  Debian packaging; performance marks (MEADOW_PERF_LOG) and budget test
- Phase 9: GTK 3/4 and Kvantum themes generated from theme.json for every
  scheme, original icon, cursor and sound themes, font aliases, and
  session-scoped application defaults (dconf profile, XDG config dirs)
- Phase 8: desktop (Meadow Hills wallpaper, icons, context menus), lock
  screen (ext-session-lock-v1, meadow-auth-helper, /etc/pam.d/meadow-lock),
  screen saver, Display Properties, Control Panel and applets, Alt+Tab,
  polkit authentication agent
- Phase 7: Start menu (XDG menus, pinned and frequent programs, places,
  recent documents), Run, Log Off and Turn Off Computer dialogs
- Phase 6: taskbar, window list, notification area, notifications, clock,
  volume, network, battery, language bar and Taskbar Properties
- Phase 1 skeleton: build system, theme tokens, stubs
