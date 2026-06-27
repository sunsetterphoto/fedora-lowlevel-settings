add_executable(fcse-helper
    fcsehelper.cpp
    fcsehelper.h
)

target_link_libraries(fcse-helper
    fcse-common
    KF6::AuthCore
    Qt6::Core
    Qt6::DBus
)

kauth_install_actions(org.kde.fcse fcse.actions)
kauth_install_helper_files(fcse-helper org.kde.fcse root)
install(TARGETS fcse-helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
