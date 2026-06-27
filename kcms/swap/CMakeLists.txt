kcmutils_add_qml_kcm(kcm_fls_swap
    SOURCES
        swapkcm.cpp swapkcm.h
)

target_link_libraries(kcm_fls_swap PRIVATE
    fls-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    KF6::AuthCore
    Qt6::Quick
)
