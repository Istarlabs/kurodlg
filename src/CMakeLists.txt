# SPDX-FileCopyrightText: 2026 Pascal Klose
# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_dbus_adaptor(kurodlg_adaptor_SRCS
    net.kurokyuu.kurodlg.ProgressDialog.xml
    progresscontroller.h
    ProgressController
)

add_executable(kurodlg
    main.cpp
    fieldspec.cpp
    fieldspec.h
    formcontroller.cpp
    formcontroller.h
    presetspec.cpp
    presetspec.h
    progresscontroller.cpp
    progresscontroller.h
    progressjob.cpp
    progressjob.h
    ${kurodlg_adaptor_SRCS}
)

# The windows. The module name is what main() loads them by. QTP0001 puts the module under :/qt/qml, the
# import path every Qt Quick application searches by default.
qt_policy(SET QTP0001 NEW)
qt_add_qml_module(kurodlg
    URI net.kurokyuu.kurodlg
    VERSION 1.0
    QML_FILES
        qml/AboutWindow.qml
        qml/FormWindow.qml
        qml/ProgressWindow.qml
)

target_compile_definitions(kurodlg PRIVATE KURODLG_VERSION="${PROJECT_VERSION}")

target_link_libraries(kurodlg PRIVATE
    Qt6::Widgets
    Qt6::DBus
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::CoreAddons
    KF6::Crash
    KF6::I18n
    KF6::I18nQml
    KF6::JobWidgets
    KF6::Notifications
    KF6::IconThemes
    KF6::WindowSystem
)

install(TARGETS kurodlg ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES net.kurokyuu.kurodlg.ProgressDialog.xml
        DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})

# NoDisplay, so it is not an entry in the application menu: this exists because the panel matches a
# window to a task through its desktop entry, and only a matched task can carry a progress bar.
# The full path in Exec: a Plasma session does not have ~/.local/bin in its PATH, and GLib drops a desktop
# entry whose program it cannot find - the portal then reports "App info not found for 'kurodlg'".
configure_file(kurodlg.desktop.in kurodlg.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kurodlg.desktop DESTINATION ${KDE_INSTALL_APPDIR})
