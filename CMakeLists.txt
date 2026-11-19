cmake_minimum_required(VERSION 3.16)

project(EmeraldMaster VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        cellwidget.cpp
        cellwidget.h
        appicons.cpp
        appicons.h
        minesweeper.cpp
        minesweeper.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(EmeraldMaster
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET EmeraldMaster APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(EmeraldMaster SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(EmeraldMaster
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(EmeraldMaster PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.EmeraldMaster)
endif()
set_target_properties(EmeraldMaster PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(UNIX AND NOT APPLE AND NOT ANDROID)
    # CMake strips the build-tree RPATH on install. This Qt kit lives outside
    # the system library paths, so keep the linked library directories in the
    # installed binary; otherwise the desktop entry cannot start it.
    set_target_properties(EmeraldMaster PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

if(ANDROID)
    # Bump the version code for every published release so devices can update
    # in place. versionName is the human-readable version.
    set_target_properties(EmeraldMaster PROPERTIES
        QT_ANDROID_VERSION_CODE 6
        QT_ANDROID_VERSION_NAME "0.4.0")
endif()

include(GNUInstallDirs)

# Linux desktop integration. Platform-guarded so a future Windows packaging
# target can keep using the same install(TARGETS) rules unchanged.
if(UNIX AND NOT APPLE)
    set(EMERALDMASTER_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/EmeraldMaster")

    # An absolute icon path bypasses the icon-theme lookups GNOME Shell caches
    # between sessions; sized copies are still installed as a fallback.
    set(EMERALDMASTER_ICON
        "${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/256x256/apps/emeraldmaster.png")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/emeraldmaster.desktop.in"
        "${CMAKE_CURRENT_BINARY_DIR}/emeraldmaster.desktop"
        @ONLY
    )

    # Install the icons before the launcher so it never exists without one.
    foreach(size 16 24 32 48 64 128)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/icons/emeraldmaster-${size}.png"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps"
            RENAME emeraldmaster.png)
    endforeach()
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/icons/emeraldmaster.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps"
        RENAME emeraldmaster.png)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/emeraldmaster.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

    # Refresh the caches desktop environments use to find apps and icons.
    find_program(UPDATE_DESKTOP_DATABASE NAMES update-desktop-database)
    if(UPDATE_DESKTOP_DATABASE)
        install(CODE "execute_process(COMMAND \"${UPDATE_DESKTOP_DATABASE}\" \"${CMAKE_INSTALL_FULL_DATADIR}/applications\" OUTPUT_QUIET ERROR_QUIET)")
    endif()
    find_program(GTK_UPDATE_ICON_CACHE NAMES gtk-update-icon-cache)
    if(GTK_UPDATE_ICON_CACHE)
        install(CODE "execute_process(COMMAND \"${GTK_UPDATE_ICON_CACHE}\" -f -t \"${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor\" OUTPUT_QUIET ERROR_QUIET)")
    endif()
endif()

install(TARGETS EmeraldMaster
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(EmeraldMaster)
endif()
