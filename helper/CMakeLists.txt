add_executable(nsa-fsm-helper
    nsafsmhelper.cpp
    nsafsmhelper.h
)

target_link_libraries(nsa-fsm-helper
    nsa-fsm-common
    KF6::AuthCore
    Qt6::Core
    Qt6::DBus
)

kauth_install_actions(org.kde.nsafsm nsa-fsm.actions)
kauth_install_helper_files(nsa-fsm-helper org.kde.nsafsm root)
install(TARGETS nsa-fsm-helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
