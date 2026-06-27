kcmutils_add_qml_kcm(kcm_fcse_sysctl
    SOURCES
        sysctlkcm.cpp sysctlkcm.h
        sysctlmodel.cpp sysctlmodel.h
)

target_link_libraries(kcm_fcse_sysctl PRIVATE
    fcse-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    KF6::AuthCore
    Qt6::Quick
)
