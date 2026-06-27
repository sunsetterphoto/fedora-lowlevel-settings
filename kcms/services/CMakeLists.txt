kcmutils_add_qml_kcm(kcm_fcse_services
    SOURCES
        serviceskcm.cpp serviceskcm.h
        unitmodel.cpp unitmodel.h
)

target_link_libraries(kcm_fcse_services PRIVATE
    fcse-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    Qt6::Quick
    Qt6::DBus
)
