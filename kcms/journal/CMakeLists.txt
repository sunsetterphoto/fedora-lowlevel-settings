kcmutils_add_qml_kcm(kcm_fcse_journal
    SOURCES
        journalkcm.cpp journalkcm.h
        journalmodel.cpp journalmodel.h
)

target_link_libraries(kcm_fcse_journal PRIVATE
    fcse-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    Qt6::Quick
    PkgConfig::SYSTEMD
)
