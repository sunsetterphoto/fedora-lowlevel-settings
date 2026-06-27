kcmutils_add_qml_kcm(kcm_fls_fstab
    SOURCES
        fstabkcm.cpp fstabkcm.h
)

target_link_libraries(kcm_fls_fstab PRIVATE
    fls-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    KF6::AuthCore
    Qt6::Quick
    Qt6::DBus
)
