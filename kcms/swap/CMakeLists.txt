kcmutils_add_qml_kcm(kcm_nsa_swap
    SOURCES
        swapkcm.cpp swapkcm.h
)

target_link_libraries(kcm_nsa_swap PRIVATE
    nsa-fsm-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    KF6::AuthCore
    Qt6::Quick
)
