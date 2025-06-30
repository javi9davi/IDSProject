cmake_minimum_required(VERSION 3.16)

cmake_policy(SET CMP0071 NEW)
project(IDS-TFG VERSION 0.1 LANGUAGES CXX)

# ==================== Configuración del compilador ====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)


# ==================== Dependencias ====================

# Libvirt
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBVIRT REQUIRED libvirt)
include_directories(${LIBVIRT_INCLUDE_DIRS})
link_directories(${LIBVIRT_LIBRARY_DIRS})

# GLib (para GHashTable)
pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS})

# OpenSSL
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Hilos
find_package(Threads REQUIRED)

#API
find_package(CURL REQUIRED)

# Qt (Widgets y traducción)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)
message(STATUS "Found Qt version: ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")

set(ENABLE_KVM ON CACHE BOOL "" FORCE)
set(ENABLE_XEN OFF CACHE BOOL "" FORCE)
set(ENABLE_BAREFLANK OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC OFF CACHE BOOL "" FORCE)
set(ENABLE_FREEBSD OFF CACHE BOOL "" FORCE)
set(ENABLE_OSX OFF CACHE BOOL "" FORCE)


# ==================== Archivos fuente ====================
set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/mainwindow.ui

        src/core/config.cpp
        src/core/config.h
        src/core/logger.cpp
        src/core/logger.h
        src/core/utils.cpp
        src/core/utils.h

        src/initializeTest/initialize.cpp
        src/initializeTest/initialize.h

        src/monitor/monitor.cpp
        src/monitor/monitor.h
        src/monitor/syscalls.cpp
        src/monitor/syscalls.h
        src/monitor/fileMonitor/fileMonitor.cpp
        src/monitor/fileMonitor/fileMonitor.h
        src/monitor/processMonitor/processMonitor.cpp
        src/monitor/processMonitor/processMonitor.h

        src/detection/signatures.cpp
        src/detection/signatures.h

        src/response/mitigation.cpp
        src/response/mitigation.h
        src/response/AlertServer.cpp
        src/response/AlertServer.h
        src/monitor/MonitorThread.cpp
        src/monitor/MonitorThread.h
        src/monitorui.h
        src/monitorui.cpp
)

# ==================== Compilación del ejecutable ====================
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(IDS-TFG
            MANUAL_FINALIZATION
            ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(IDS-TFG SHARED ${PROJECT_SOURCES})
    else()
        add_executable(IDS-TFG ${PROJECT_SOURCES})
    endif()
endif()

# ==================== Enlazado de bibliotecas ====================
target_link_libraries(IDS-TFG PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt6::Network
        ${OPENSSL_LIBRARIES}
        ${LIBVIRT_LIBRARIES}
        virt-qemu
        json-c
        Threads::Threads
        ${GLIB2_LIBRARIES}
        CURL::libcurl
)


# Necesario para std::filesystem en GCC <= 8
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(IDS-TFG PRIVATE stdc++fs)
endif()

# Flags extra de compilación
target_compile_options(IDS-TFG PRIVATE ${LIBVIRT_CFLAGS_OTHER})

# ==================== Instalación ====================
set_target_properties(IDS-TFG PROPERTIES
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS IDS-TFG
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Finalización para Qt 6
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(IDS-TFG)
endif()
