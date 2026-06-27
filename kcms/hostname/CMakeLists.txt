kcmutils_add_qml_kcm(kcm_fcse_hostname
    SOURCES
        hostnamekcm.cpp hostnamekcm.h
)

target_link_libraries(kcm_fcse_hostname PRIVATE
    fcse-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    KF6::AuthCore
    Qt6::Quick
    Qt6::DBus
)
