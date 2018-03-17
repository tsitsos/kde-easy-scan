set(kEasySkan_SRCS main.cpp kEasySkan.cpp ImageViewer.cpp KSaneImageSaver.cpp )

ki18n_wrap_ui(kEasySkan_SRCS settings.ui )


add_executable(kEasySkan ${kEasySkan_SRCS})

target_link_libraries(kEasySkan
  
    Qt5::Core
    Qt5::PrintSupport
    KF5::CoreAddons
    KF5::Sane
    KF5::I18n
    KF5::XmlGui
    KF5::KIOWidgets
    KF5::ConfigCore
    KF5::I18n
    KF5::KIOWidgets
    KF5::XmlGui
    ${PNG_LIBRARY}
)

install(TARGETS kEasySkan ${INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.kEasySkan.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install( FILES org.kde.kEasySkan.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
install (DIRECTORY help DESTINATION ${CMAKE_INSTALL_PREFIX}/share/kEasySkan)
install (DIRECTORY icons DESTINATION ${CMAKE_INSTALL_PREFIX}/share/kEasySkan)
