kcmutils_add_qml_kcm(kcm_fls_dnfrepos
    SOURCES
        dnfreposkcm.cpp dnfreposkcm.h
)

target_link_libraries(kcm_fls_dnfrepos PRIVATE
    fls-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    KF6::AuthCore
    Qt6::Quick
)
