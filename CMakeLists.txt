cmake_minimum_required(VERSION 3.16)
project(ClamSecurity VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Concurrent)

set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/managers/ClamAvManager.h
    src/managers/ClamAvManager.cpp
    src/managers/QuarantineManager.h
    src/managers/QuarantineManager.cpp
    src/managers/UFWManager.h
    src/managers/UFWManager.cpp
    src/managers/SystemChecker.h
    src/managers/SystemChecker.cpp
    src/managers/AutostartManager.h
    src/managers/AutostartManager.cpp
    src/workers/ScanWorker.h
    src/workers/ScanWorker.cpp
    src/core/NotificationService.h
    src/core/NotificationService.cpp
    src/ui/MonitorWidget.h
    src/ui/MonitorWidget.cpp
    src/ui/ModuleCard.h
    src/ui/ModuleCard.cpp
    src/ui/DetailsDialog.h
    src/ui/DetailsDialog.cpp
    src/ui/ScanPage.h
    src/ui/ScanPage.cpp
    src/ui/DatabasePage.h
    src/ui/DatabasePage.cpp
    src/ui/ExclusionsPage.h
    src/ui/ExclusionsPage.cpp
    src/ui/QuarantinePage.h
    src/ui/QuarantinePage.cpp
    src/ui/FirewallPage.h
    src/ui/FirewallPage.cpp
    src/ui/SettingsPage.h
    src/ui/SettingsPage.cpp
    resources/ClamSecurity.qrc
)

add_executable(ClamSecurity ${PROJECT_SOURCES})

target_include_directories(ClamSecurity PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/managers
    ${CMAKE_SOURCE_DIR}/src/workers
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/ui
)

target_link_libraries(ClamSecurity PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Concurrent
)

include(GNUInstallDirs)
install(TARGETS ClamSecurity DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES scripts/org.kde.clamsecurity.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/kio/servicemenus/)
