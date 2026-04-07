cmake_minimum_required(VERSION 3.22)
project(nsa-fedora-system-manager VERSION 0.1.0)

set(QT_MIN_VERSION "6.8.0")
set(KF6_MIN_VERSION "6.22.0")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core Quick DBus
)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons I18n Auth KCMUtils Config Service DBusAddons Notifications
)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

add_subdirectory(common)
add_subdirectory(helper)
add_subdirectory(kcms)

# Install category desktop file
install(FILES categories/nsa-system-administration.desktop
        DESTINATION ${KDE_INSTALL_DATADIR}/systemsettings/categories)

# Backup directory creation
install(DIRECTORY DESTINATION /var/lib/nsa-fsm/backups)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
