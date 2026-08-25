include(CMakeFindDependencyMacro)
find_dependency(Qt6 6.5 REQUIRED COMPONENTS Core Network)
include("${CMAKE_CURRENT_LIST_DIR}/YaapProviderSdkTargets.cmake")
