cmake_minimum_required(VERSION 3.16)
project(ClamSecurity VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Concurrent Network LinguistTools)

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
    src/managers/ClamdConfigManager.h
    src/managers/ClamdConfigManager.cpp
    src/workers/ScanWorker.h
    src/workers/ScanWorker.cpp
    src/core/NotificationService.h
    src/core/NotificationService.cpp
    src/core/LanguageManager.h
    src/core/LanguageManager.cpp
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
    src/ui/ActiveThreatsPage.h
    src/ui/ActiveThreatsPage.cpp
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
    Qt6::Network
)

# ── Internationalization ─────────────────────────────────────────────────────
# Source language: English  (no .ts file needed for English)
# Add new languages here — Qt Linguist will generate the .qm from the .ts file.
set(TS_FILES
    translations/ClamSecurity_es.ts
)

# qt_add_lupdate: updates .ts from source (run with cmake --build . --target update_translations)
qt_add_lupdate(ClamSecurity TS_FILES ${TS_FILES} NO_GLOBAL_TARGET)

# qt_add_lrelease: compiles .ts → .qm and embeds them as Qt resources
qt_add_lrelease(ClamSecurity
    TS_FILES ${TS_FILES}
    QM_FILES_OUTPUT_VARIABLE QM_FILES
)
qt_add_resources(ClamSecurity "app_translations"
    PREFIX "/translations"
    BASE   "${CMAKE_CURRENT_BINARY_DIR}"
    FILES  ${QM_FILES}
)

# ── Install ──────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS ClamSecurity DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES scripts/org.kde.clamsecurity.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/kio/servicemenus/)
