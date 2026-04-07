set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(nsa-fsm-common STATIC
    backupmanager.cpp
    backupmanager.h
    dbushelper.cpp
    dbushelper.h
    configparser.cpp
    configparser.h
    statusnotifier.cpp
    statusnotifier.h
)

target_link_libraries(nsa-fsm-common PUBLIC
    Qt6::Core
    Qt6::DBus
    KF6::CoreAddons
    KF6::Notifications
)

target_include_directories(nsa-fsm-common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
