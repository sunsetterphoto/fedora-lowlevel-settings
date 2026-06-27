add_library(fls-helperpolicy STATIC
    helperpolicy.cpp
    helperpolicy.h
)
set_target_properties(fls-helperpolicy PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(fls-helperpolicy PUBLIC Qt6::Core)
target_include_directories(fls-helperpolicy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(fls-helper
    flshelper.cpp
    flshelper.h
)

target_link_libraries(fls-helper
    fls-common
    fls-helperpolicy
    KF6::AuthCore
    Qt6::Core
    Qt6::DBus
)

kauth_install_actions(org.kde.fls fls.actions)
kauth_install_helper_files(fls-helper org.kde.fls root)
install(TARGETS fls-helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
