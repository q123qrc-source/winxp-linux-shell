# SPDX-License-Identifier: GPL-3.0-or-later
# Third-party app theming. Contents are filled in phase 9; phase 1 installs the
# index/metadata files so packaging paths are fixed from the start.
set(themes_dir ${CMAKE_INSTALL_DATADIR}/themes/Meadow-Luna)

install(FILES gtk-3.0/gtk.css DESTINATION ${themes_dir}/gtk-3.0)
install(FILES gtk-4.0/gtk.css DESTINATION ${themes_dir}/gtk-4.0)
install(FILES kvantum/MeadowLuna/MeadowLuna.kvconfig kvantum/MeadowLuna/MeadowLuna.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/Kvantum/MeadowLuna)
install(FILES icons/meadow-icons/index.theme DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/meadow-icons)
install(FILES cursors/meadow-cursors/index.theme DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/meadow-cursors)
install(FILES sounds/meadow/index.theme DESTINATION ${CMAKE_INSTALL_DATADIR}/sounds/meadow)
