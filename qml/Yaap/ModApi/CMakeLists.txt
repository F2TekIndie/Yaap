qt_add_library(yaap_mod_api_qml STATIC)
qt_add_qml_module(
  yaap_mod_api_qml
  URI Yaap.ModApi
  VERSION 1.1
)
target_link_libraries(yaap_mod_api_qml PRIVATE Qt6::Core Qt6::Qml)
yaap_enable_warnings(yaap_mod_api_qml)
