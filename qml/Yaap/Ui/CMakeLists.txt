qt_add_library(yaap_ui STATIC)
qt_add_qml_module(
  yaap_ui
  URI Yaap.Ui
  VERSION 1.0
  QML_FILES
    AnimatedWaveBackground.qml
    AnimatedPaperPlaneBackground.qml
    ExtensionHost.qml
    FramelessDialog.qml
    SpectrumBackground.qml
    ThemedPanel.qml
)
target_link_libraries(yaap_ui PRIVATE Qt6::Core Qt6::Qml Qt6::Quick)
yaap_enable_warnings(yaap_ui)
