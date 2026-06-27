kcmutils_add_qml_kcm(kcm_fls_journal
    SOURCES
        journalkcm.cpp journalkcm.h
        journalmodel.cpp journalmodel.h
)

target_link_libraries(kcm_fls_journal PRIVATE
    fls-common
    KF6::KCMUtils
    KF6::I18n
    KF6::CoreAddons
    Qt6::Quick
    PkgConfig::SYSTEMD
)
