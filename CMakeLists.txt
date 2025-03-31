cmake_minimum_required(VERSION 3.16)

project(IDS-TFG VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(/home/javierdr/libvmi)
link_directories(/home/javierdr/libvmi/build)

# Encontrar y configurar libvirt
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBVIRT REQUIRED libvirt)

find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
link_directories(${OPENSSL_LIBRARIES})

include_directories(${LIBVIRT_INCLUDE_DIRS})
link_directories(${LIBVIRT_LIBRARY_DIRS})

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)
message(STATUS "Found Qt version: ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}.${QT_VERSION_PATCH}")

find_library(VMI_LIBRARY NAMES vmi PATHS /home/javierdr/libvmi/build/libvmi)

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/mainwindow.ui
        src/monitor/monitor.cpp
        src/monitor/monitor.h
        src/initializeTest/initialize.cpp
        src/initializeTest/initialize.h
        src/core/config.cpp
        src/core/logger.cpp
        src/core/utils.cpp
        src/core/config.h
        src/core/logger.h
        src/core/utils.h
        src/monitor/vmi.cpp
        src/monitor/vmi.h
        src/monitor/process.cpp
        src/monitor/process.h
        src/monitor/syscalls.cpp
        src/monitor/syscalls.h
        src/detection/signatures.cpp
        src/detection/signatures.h
        src/detection/anomaly.cpp
        src/detection/anomaly.h
        src/response/mitigation.cpp
        src/response/mitigation.h
        src/response/alerting.cpp
        src/response/alerting.h
        src/monitor/fileMonitor/fileMonitor.cpp
        src/monitor/fileMonitor/fileMonitor.h
)

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

# Enlazar las bibliotecas
target_link_libraries(IDS-TFG PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::DBus
        ${OPENSSL_LIBRARIES}
        ${VMI_LIBRARY}
        ${LIBVIRT_LIBRARIES}
)

target_compile_options(IDS-TFG PRIVATE ${LIBVIRT_CFLAGS_OTHER})

# Configuración del paquete
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

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(IDS-TFG)
endif()
