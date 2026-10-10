# SPDX-License-Identifier: GPL-3.0-or-later
# Fedora packaging for Meadow. Primary distribution target.
#
# Policy: installing or removing these packages never changes how the system
# logs in. No scriptlet touches /etc, PAM, authselect or the display manager;
# that is done only on request by `meadow-setup` (see README-fedora.md).

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
BuildRequires:  cmake(Qt6Test)
BuildRequires:  cmake(Qt6WaylandClient)
BuildRequires:  cmake(Qt6Multimedia)
BuildRequires:  cmake(Qt6LinguistTools)
BuildRequires:  cmake(LayerShellQt)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(wayland-protocols)
BuildRequires:  polkit-qt6-1-devel
BuildRequires:  desktop-file-utils
BuildRequires:  librsvg2-tools
BuildRequires:  xcursorgen
BuildRequires:  python3
# Needed by %%ctest: the QML smoke test renders offscreen.
BuildRequires:  qt6-qtbase-gui

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

%description    common
The Meadow theme loader, sound player and the Meadow.Controls QML module,
shared by the Meadow shell and the Meadow SDDM theme.

%package        shell
Summary:        Meadow desktop shell, session files and setup tool
License:        GPL-3.0-or-later
Requires:       %{name}-common%{?_isa} = %{version}-%{release}
Requires:       %{name}-themes = %{version}-%{release}
Requires:       labwc
Requires:       xorg-x11-server-Xwayland
Requires:       xdg-desktop-portal-wlr
Requires:       xdg-desktop-portal-gtk
Requires:       pipewire
Requires:       wireplumber
Requires:       NetworkManager
Requires:       upower
Requires:       pcmanfm-qt
Requires:       kvantum

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
Requires:       %{name}-common = %{version}-%{release}
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
Meadow's Luna Blue, Olive Green and Silver scheme definitions plus the
Meadow-Luna GTK and labwc themes, the MeadowLuna Kvantum theme, the
meadow-icons and meadow-cursors themes, the Meadow sound theme and a
fontconfig snippet mapping classic UI font names to open fonts.

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
# Screenshot comparisons depend on the exact fonts installed; they run in CI only.
%ctest --label-exclude screenshot

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

%files shell
%license LICENSE LICENSES/*
%doc README.md docs/ARCHITECTURE.md docs/PAM_FINGERPRINT.md docs/ALTERNATIVES.md
%doc packaging/fedora/README-fedora.md
%{_bindir}/meadow-shell
%{_bindir}/meadow-session
%{_bindir}/meadow-setup
%dir %{_datadir}/wayland-sessions
%{_datadir}/wayland-sessions/meadow.desktop
%{_datadir}/xdg-desktop-portal/meadow-portals.conf
%dir %{_datadir}/%{name}
%{_datadir}/%{name}/labwc/
%{_datadir}/%{name}/sddm.conf.d/
%{_datadir}/%{name}/pam.d/
%{_datadir}/%{name}/polkit/

%files -n sddm-theme-%{name}
%license LICENSE LICENSES/*
%doc README.md
%{_datadir}/sddm/themes/%{name}/

%files themes
%license LICENSE LICENSES/*
%doc docs/VISUAL_SPEC.md
%dir %{_datadir}/%{name}
%{_datadir}/%{name}/themes/
%{_datadir}/themes/Meadow-Luna/
%dir %{_datadir}/Kvantum
%{_datadir}/Kvantum/MeadowLuna/
%{_datadir}/icons/meadow-icons/
%{_datadir}/icons/meadow-cursors/
%dir %{_datadir}/sounds
%{_datadir}/sounds/meadow/
%{_datadir}/fontconfig/conf.avail/50-meadow.conf

%changelog
* Sat Oct 10 2026 Meadow contributors <q123qrc@gmail.com> - 0.1.0-1
- Phase 1 skeleton: build system, theme tokens, stubs
