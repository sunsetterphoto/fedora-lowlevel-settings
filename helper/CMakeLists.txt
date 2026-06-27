add_executable(fls-helper
    flshelper.cpp
    flshelper.h
)

target_link_libraries(fls-helper
    fls-common
    KF6::AuthCore
    Qt6::Core
    Qt6::DBus
)

kauth_install_actions(org.kde.fls fls.actions)
kauth_install_helper_files(fls-helper org.kde.fls root)
install(TARGETS fls-helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
