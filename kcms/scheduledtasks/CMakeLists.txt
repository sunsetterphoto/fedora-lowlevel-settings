kcmutils_add_qml_kcm(kcm_fls_scheduledtasks
    SOURCES
        scheduledtaskskcm.cpp scheduledtaskskcm.h
)

target_link_libraries(kcm_fls_scheduledtasks PRIVATE
    fls-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    Qt6::Quick
    Qt6::DBus
)
