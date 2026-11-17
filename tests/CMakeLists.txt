# SPDX-FileCopyrightText: 2026 Pascal Klose
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(fieldspectest
    fieldspectest.cpp
    ../src/fieldspec.cpp
    ../src/fieldspec.h
)

target_include_directories(fieldspectest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(fieldspectest PRIVATE Qt6::Test KF6::I18n)

add_test(NAME fieldspectest COMMAND fieldspectest)

add_executable(presetspectest
    presetspectest.cpp
    ../src/fieldspec.cpp
    ../src/fieldspec.h
    ../src/presetspec.cpp
    ../src/presetspec.h
)

target_include_directories(presetspectest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(presetspectest PRIVATE Qt6::Test KF6::I18n)

add_test(NAME presetspectest COMMAND presetspectest)

add_executable(formcontrollertest
    formcontrollertest.cpp
    ../src/fieldspec.cpp
    ../src/fieldspec.h
    ../src/formcontroller.cpp
    ../src/formcontroller.h
    ../src/presetspec.cpp
    ../src/presetspec.h
)

target_include_directories(formcontrollertest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(formcontrollertest PRIVATE Qt6::Test Qt6::Qml KF6::I18n)

add_test(NAME formcontrollertest COMMAND formcontrollertest)
