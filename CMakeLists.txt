cmake_minimum_required(VERSION 3.21)
project(ClipKeeper VERSION 1.0.0 LANGUAGES CXX)

# --- C++20 -------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Sql Network)
qt_standard_project_setup()   # AUTOMOC/AUTORCC/AUTOUIC + sensible defaults

# Exe icon: bake an absolute path into the .rc so rc.exe finds the .ico.
set(CLIPKEEPER_ICO "${CMAKE_CURRENT_SOURCE_DIR}/assets/clipkeeper.ico")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/assets/app.rc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
# Re-run the resource compiler when the .ico itself changes.
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app.rc"
                            PROPERTIES OBJECT_DEPENDS "${CLIPKEEPER_ICO}")

qt_add_executable(ClipKeeper
    "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
    src/main.cpp
    src/config.cpp     src/config.h
    src/appicon.cpp    src/appicon.h
    src/db.cpp         src/db.h
    src/guard.cpp      src/guard.h
    src/winclip.cpp    src/winclip.h
    src/capture.cpp    src/capture.h
    src/theme.cpp      src/theme.h
    src/fuzzy.cpp      src/fuzzy.h
    src/historymodel.cpp   src/historymodel.h
    src/rowdelegate.cpp    src/rowdelegate.h
    src/popup.cpp      src/popup.h
    src/hotkey.cpp     src/hotkey.h
    src/keyboardhook.cpp   src/keyboardhook.h
    src/settings.cpp   src/settings.h
    src/autostart.cpp  src/autostart.h
    src/winhistory.cpp src/winhistory.h
    src/singleinstance.cpp src/singleinstance.h
    src/setupdialog.cpp    src/setupdialog.h
    src/supabaseclient.cpp src/supabaseclient.h
    src/syncengine.cpp     src/syncengine.h
    src/accountdialog.cpp  src/accountdialog.h
)

# Keep <windows.h> from defining min()/max() macros (clash with std/Qt) and pull
# in the lean subset. Applies to the whole target.
target_compile_definitions(ClipKeeper PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

# Drop-in identity: OFF (dev) uses the isolated "ClipKeeperQt" data dir + autostart
# name so it can't disturb the Python app; ON uses the shared "ClipKeeper" names to
# become the real replacement. See config.cpp / autostart.cpp.
option(CLIPKEEPER_DROP_IN "Use the shared 'ClipKeeper' data-dir + autostart identity" OFF)
if(CLIPKEEPER_DROP_IN)
    target_compile_definitions(ClipKeeper PRIVATE CLIPKEEPER_DROP_IN)
endif()

target_include_directories(ClipKeeper PRIVATE src)
target_link_libraries(ClipKeeper PRIVATE Qt6::Widgets Qt6::Sql Qt6::Network)

# Native Win32 integration this app grows into: clipboard-format listener,
# RegisterHotKey, WH_KEYBOARD_LL hook, DWM rounded corners, CF_HDROP marshalling.
# Linked up front so later phases just #include <windows.h> and call.
if(WIN32)
    target_link_libraries(ClipKeeper PRIVATE user32 dwmapi shell32 ole32 gdi32)
endif()

# Dev builds keep a console so qDebug()/stdout show in the terminal and the
# --selftest smoke check is capturable. Phase 6 (packaging) flips this OFF for a
# windowed, no-console tray app.
option(CLIPKEEPER_CONSOLE "Build as a console app (dev; shows qDebug output)" ON)
if(NOT CLIPKEEPER_CONSOLE)
    set_target_properties(ClipKeeper PROPERTIES WIN32_EXECUTABLE ON)
endif()
