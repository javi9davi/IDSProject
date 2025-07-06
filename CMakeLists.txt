cmake_minimum_required(VERSION 3.16)

project(IDS-TFG VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------- LIBVMI --------------------
include_directories(/home/javierdr/libvmi)
link_directories(/home/javierdr/libvmi/build)

# -------------------- LIBVIRT + GLIB + JSON-C --------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBVIRT REQUIRED libvirt)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(${LIBVIRT_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS})
link_directories(${LIBVIRT_LIBRARY_DIRS} ${GLIB_LIBRARY_DIRS})

# -------------------- OPENSSL --------------------
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
link_directories(${OPENSSL_LIBRARIES})

# -------------------- Qt6 --------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2)

# -------------------- CURL --------------------
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIRS})

# -------------------- OTROS --------------------
find_package(tinyxml2 REQUIRED)
find_package(SQLite3 REQUIRED)

# -------------------- PROYECTO --------------------
set(PROJECT_SOURCES
        src/main.cpp
        src/monitorui.cpp
        src/monitorui.h
        src/mainwindow.cpp
        src/mainwindow.h
        src/mainwindow.ui
        src/monitor/MonitorThread.cpp
        src/monitor/MonitorThread.h
        src/monitor/processMonitor/processMonitor.cpp
        src/monitor/processMonitor/processMonitor.h
        src/monitor/monitor.cpp
        src/monitor/monitor.h
        src/monitor/vmi.cpp
        src/monitor/vmi.h
        src/monitor/syscalls.cpp
        src/monitor/syscalls.h
        src/monitor/fileMonitor/fileMonitor.cpp
        src/monitor/fileMonitor/fileMonitor.h
        src/initializeTest/initialize.cpp
        src/initializeTest/initialize.h
        src/core/config.cpp
        src/core/config.h
        src/core/logger.cpp
        src/core/logger.h
        src/core/utils.cpp
        src/core/utils.h
        src/detection/signatures.cpp
        src/detection/signatures.h
        src/response/mitigation.cpp
        src/response/mitigation.h
        src/backend/ids_backend_service.cpp
        src/backend/ids_backend_service.h
        src/initializeTest/vm_utils.cpp
        src/initializeTest/vm_utils.h
        src/backend/SqliteHashDB.cpp
        src/backend/SqliteHashDB.h
        src/monitor/processMonitor/dumpGuestFile.cpp
)

# -------------------- INCLUDES DEL PROYECTO --------------------
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/monitor
        ${CMAKE_CURRENT_SOURCE_DIR}/src/monitor/fileMonitor
        ${CMAKE_CURRENT_SOURCE_DIR}/src/initializeTest
        ${CMAKE_CURRENT_SOURCE_DIR}/src/detection
        ${CMAKE_CURRENT_SOURCE_DIR}/src/response
        ${CMAKE_CURRENT_SOURCE_DIR}/src/backend
)

# -------------------- COMPILACIÓN --------------------
qt_add_executable(IDS-TFG
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
)

target_compile_options(IDS-TFG PRIVATE ${LIBVIRT_CFLAGS_OTHER})

target_link_libraries(IDS-TFG PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        ${OPENSSL_LIBRARIES}
        ${LIBVIRT_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${CURL_LIBRARIES}
        virt-qemu
        json-c
        tinyxml2
        SQLite::SQLite3
)

set_target_properties(IDS-TFG PROPERTIES
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(IDS-TFG)

# -------------------- INSTALACIÓN --------------------
include(GNUInstallDirs)
install(TARGETS IDS-TFG
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
