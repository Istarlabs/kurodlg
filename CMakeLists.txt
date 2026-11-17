# SPDX-FileCopyrightText: 2026 Pascal Klose
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.20)

project(kurodlg VERSION 0.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION 6.7.0)
# 6.8 is where KLocalizedContext was replaced by KLocalizedQmlContext.
set(KF_MIN_VERSION 6.8.0)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

# Deliberately no include(KDEClangFormat): it rewrites .clang-format at every configure with ECM's
# copy, and this repository's style differs from it in one respect - see the file itself. The
# formatting target it also provides is not needed, Scripts/cpp.py format runs clang-format itself.

# KDECompilerSettings pins the standard to 17, so this has to come after it.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Widgets DBus Qml Quick QuickControls2 Test)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    Crash
    I18n
    JobWidgets
    Notifications
    IconThemes
    WindowSystem
    Kirigami
)

# The windows are QML. Kirigami and kirigami-addons are only loaded at run time, but a build that cannot
# find them would produce a program that fails on its first window, so they are checked here.
find_package(KF6KirigamiAddons REQUIRED)

add_subdirectory(src)

# Compiles every po/<lang>/kurodlg.po to a .mo and installs it where KLocalizedString looks. The
# domain it is found under is the one main() passes to setApplicationDomain().
ki18n_install(po)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
