cmake_minimum_required(VERSION 4.2)

project(
  Yaap
  VERSION 0.1.0
  DESCRIPTION "Cross-platform moddable music player"
  LANGUAGES CXX
)

option(YAAP_BUILD_APP "Build the Qt Quick desktop application" ON)
option(YAAP_ENABLE_SANITIZERS "Enable supported address/undefined sanitizers" OFF)
option(YAAP_BUILD_FUZZERS "Build libFuzzer parser targets with Clang" OFF)
option(YAAP_REQUIRE_SIGNING "Fail release metadata checks without a signing identity" OFF)
set(YAAP_SIGNING_IDENTITY "" CACHE STRING "Platform signing certificate thumbprint or identity")

if(YAAP_ENABLE_SANITIZERS)
  if(MSVC)
    add_compile_options(/fsanitize=address)
  else()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

set(
  YAAP_DISTRIBUTION_ROOT
  "${CMAKE_BINARY_DIR}/distribution"
  CACHE PATH
  "Directory where runnable post-build application layouts are created"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

function(yaap_enable_warnings target)
  if(MSVC)
    # Keep project warnings strict while treating SDK/dependency headers as external.
    target_compile_options(
      ${target}
      PRIVATE /W4 /permissive- /Zc:__cplusplus /external:anglebrackets /external:W0
    )
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
  endif()
endfunction()

add_library(
  yaap_core
  src/core/AsyncTaskReaper.cpp
  src/core/AsyncTaskReaper.hpp
  src/core/PlaybackState.cpp
  src/core/PlaybackState.hpp
  src/domain/PlaybackQueue.cpp
  src/domain/PlaybackQueue.hpp
  src/domain/NowPlayingMetadata.hpp
  src/domain/PlaybackSession.hpp
  src/domain/Playlist.hpp
  src/domain/Provider.hpp
  src/domain/Track.hpp
)
add_library(Yaap::Core ALIAS yaap_core)
target_include_directories(yaap_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(yaap_core PUBLIC cxx_std_20)
yaap_enable_warnings(yaap_core)

include(CTest)

if(YAAP_BUILD_APP OR BUILD_TESTING)
  find_package(FFmpeg REQUIRED)
  find_path(MINIAUDIO_INCLUDE_DIR NAMES miniaudio.h REQUIRED)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Core Concurrent Gui Network Qml Sql)

  add_library(
    yaap_extension_api
    src/extension_api/ApiVersion.cpp
    src/extension_api/ApiVersion.hpp
    src/extension_api/ModManifest.cpp
    src/extension_api/ModManifest.hpp
    src/extension_api/ModPermission.cpp
    src/extension_api/ModPermission.hpp
    src/extension_api/ProviderProtocol.hpp
  )
  add_library(Yaap::ExtensionApi ALIAS yaap_extension_api)
  set_target_properties(yaap_extension_api PROPERTIES EXPORT_NAME ExtensionApi)
  target_include_directories(
    yaap_extension_api
    PUBLIC
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
      "$<INSTALL_INTERFACE:include>"
  )
  target_link_libraries(yaap_extension_api PUBLIC Qt6::Core)
  target_compile_features(yaap_extension_api PUBLIC cxx_std_20)
  yaap_enable_warnings(yaap_extension_api)

  add_library(
    yaap_provider_protocol
    src/provider_protocol/ProviderProtocolCodec.cpp
    src/provider_protocol/ProviderProtocolCodec.hpp
  )
  add_library(Yaap::ProviderProtocol ALIAS yaap_provider_protocol)
  set_target_properties(yaap_provider_protocol PROPERTIES EXPORT_NAME ProviderProtocol)
  target_include_directories(
    yaap_provider_protocol
    PUBLIC
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
      "$<INSTALL_INTERFACE:include>"
  )
  target_link_libraries(yaap_provider_protocol PUBLIC Yaap::ExtensionApi Qt6::Core)
  yaap_enable_warnings(yaap_provider_protocol)

  if(YAAP_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      message(FATAL_ERROR "YAAP_BUILD_FUZZERS requires Clang or AppleClang")
    endif()
    add_executable(yaap_protocol_fuzzer tests/fuzz/ProviderProtocolFuzzer.cpp)
    target_link_libraries(yaap_protocol_fuzzer PRIVATE Yaap::ProviderProtocol)
    target_compile_options(yaap_protocol_fuzzer PRIVATE -fsanitize=fuzzer,address)
    target_link_options(yaap_protocol_fuzzer PRIVATE -fsanitize=fuzzer,address)
  endif()

  add_library(
    yaap_mod_runtime
    src/mods/ExtensionRegistry.cpp
    src/mods/ExtensionRegistry.hpp
    src/mods/ModApiInfo.cpp
    src/mods/ModApiInfo.hpp
    src/mods/ModManager.cpp
    src/mods/ModManager.hpp
    src/mods/ModManifestParser.cpp
    src/mods/ModManifestParser.hpp
    src/mods/PermissionStore.cpp
    src/mods/PermissionStore.hpp
    src/mods/ThemeManager.cpp
    src/mods/ThemeManager.hpp
  )
  add_library(Yaap::ModRuntime ALIAS yaap_mod_runtime)
  target_include_directories(yaap_mod_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(
    yaap_mod_runtime
    PUBLIC Yaap::ExtensionApi Qt6::Core Qt6::Gui
  )
  yaap_enable_warnings(yaap_mod_runtime)

  add_library(
    yaap_provider_runtime
    src/provider_runtime/ProviderExtensionManager.cpp
    src/provider_runtime/ProviderExtensionManager.hpp
    src/provider_runtime/ProviderGateway.cpp
    src/provider_runtime/ProviderGateway.hpp
    src/provider_runtime/ProviderCache.cpp
    src/provider_runtime/ProviderCache.hpp
    src/provider_runtime/ProviderProcessSupervisor.cpp
    src/provider_runtime/ProviderProcessSupervisor.hpp
  )
  add_library(Yaap::ProviderRuntime ALIAS yaap_provider_runtime)
  target_include_directories(yaap_provider_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(
    yaap_provider_runtime
    PUBLIC Yaap::ModRuntime Yaap::ProviderProtocol Qt6::Core Qt6::Network Qt6::Sql
  )
  yaap_enable_warnings(yaap_provider_runtime)

  add_library(
    yaap_provider_sdk
    sdk/cpp/ProviderApplication.cpp
    sdk/cpp/ProviderApplication.hpp
    sdk/cpp/ProviderService.hpp
  )
  add_library(Yaap::ProviderSdk ALIAS yaap_provider_sdk)
  set_target_properties(yaap_provider_sdk PROPERTIES EXPORT_NAME ProviderSdk)
  target_include_directories(
    yaap_provider_sdk
    PUBLIC
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
      "$<INSTALL_INTERFACE:include>"
  )
  target_link_libraries(
    yaap_provider_sdk PUBLIC Yaap::ProviderProtocol Qt6::Core Qt6::Network
  )
  yaap_enable_warnings(yaap_provider_sdk)

  add_executable(yaap_sample_provider samples/provider/main.cpp)
  set_target_properties(yaap_sample_provider PROPERTIES OUTPUT_NAME "YaapSampleProvider")
  target_link_libraries(yaap_sample_provider PRIVATE Yaap::ProviderSdk)
  yaap_enable_warnings(yaap_sample_provider)

  add_executable(yaap_provider_conformance tools/provider_conformance/main.cpp)
  set_target_properties(yaap_provider_conformance PROPERTIES
    OUTPUT_NAME "yaap-provider-conformance")
  target_link_libraries(yaap_provider_conformance PRIVATE Yaap::ProviderRuntime)
  yaap_enable_warnings(yaap_provider_conformance)

  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/YaapProviderSdkConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
  )
  install(
    TARGETS yaap_extension_api yaap_provider_protocol yaap_provider_sdk
    EXPORT YaapProviderSdkTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
  install(
    EXPORT YaapProviderSdkTargets
    FILE YaapProviderSdkTargets.cmake
    NAMESPACE Yaap::
    DESTINATION lib/cmake/YaapProviderSdk
  )
  install(
    FILES
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/YaapProviderSdkConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/YaapProviderSdkConfigVersion.cmake"
    DESTINATION lib/cmake/YaapProviderSdk
  )
  install(DIRECTORY sdk/cpp DESTINATION include/sdk FILES_MATCHING PATTERN "*.hpp")
  install(DIRECTORY src/extension_api DESTINATION include FILES_MATCHING PATTERN "*.hpp")
  install(DIRECTORY src/provider_protocol DESTINATION include FILES_MATCHING PATTERN "*.hpp")
  install(TARGETS yaap_sample_provider RUNTIME DESTINATION share/Yaap/examples)
  install(TARGETS yaap_provider_conformance RUNTIME DESTINATION bin)

  add_library(
    yaap_audio
    src/audio/FFmpegDecoder.cpp
    src/audio/FFmpegDecoder.hpp
    src/audio/MediaMetadata.cpp
    src/audio/MediaMetadata.hpp
    src/audio/MiniaudioOutput.cpp
    src/audio/MiniaudioOutput.hpp
    src/audio/PcmFormat.hpp
    src/audio/PcmStream.cpp
    src/audio/PcmStream.hpp
    src/audio/SpscPcmRingBuffer.cpp
    src/audio/SpscPcmRingBuffer.hpp
    src/audio/StreamMetadata.cpp
    src/audio/StreamMetadata.hpp
  )
  add_library(Yaap::Audio ALIAS yaap_audio)
  target_include_directories(
    yaap_audio
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
    SYSTEM PRIVATE "${MINIAUDIO_INCLUDE_DIR}"
  )
  target_link_libraries(
    yaap_audio
    PUBLIC Yaap::Core
    PRIVATE
      FFmpeg::avformat
      FFmpeg::avcodec
      FFmpeg::avutil
      FFmpeg::swresample
  )
  target_compile_features(yaap_audio PUBLIC cxx_std_20)
  yaap_enable_warnings(yaap_audio)

  add_library(
    yaap_services
    src/library/LibraryDatabase.cpp
    src/library/LibraryDatabase.hpp
    src/library/LibraryIndexer.cpp
    src/library/LibraryIndexer.hpp
    src/providers/ProviderClients.cpp
    src/providers/ProviderClients.hpp
    src/providers/ProviderAccountStore.cpp
    src/providers/ProviderAccountStore.hpp
    src/radio/RadioPlaylist.cpp
    src/radio/RadioPlaylist.hpp
    src/radio/RadioPlaylistLoader.cpp
    src/radio/RadioPlaylistLoader.hpp
    src/radio/RadioBrowserClient.cpp
    src/radio/RadioBrowserClient.hpp
    src/security/CredentialStore.cpp
    src/security/CredentialStore.hpp
    src/security/CredentialHandleBroker.cpp
    src/security/CredentialHandleBroker.hpp
  )
  add_library(Yaap::Services ALIAS yaap_services)
  target_include_directories(yaap_services PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(
    yaap_services
    PUBLIC
      Yaap::Core
      Qt6::Concurrent
      Qt6::Core
      Qt6::Network
      Qt6::Sql
    PRIVATE
      Yaap::Audio
  )
  target_compile_features(yaap_services PUBLIC cxx_std_20)
  yaap_enable_warnings(yaap_services)
  if(WIN32)
    target_link_libraries(yaap_services PRIVATE Advapi32)
  elseif(APPLE)
    find_library(YAAP_SECURITY_FRAMEWORK Security REQUIRED)
    target_link_libraries(yaap_services PRIVATE "${YAAP_SECURITY_FRAMEWORK}")
  endif()
  target_link_libraries(yaap_provider_runtime PUBLIC Yaap::Services)
endif()

if(YAAP_BUILD_APP)
  find_package(
    Qt6 6.5
    REQUIRED
    COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2
  )

  qt_standard_project_setup(REQUIRES 6.5)
  qt_policy(SET QTP0004 NEW)

  add_subdirectory(qml/Yaap/ModApi)
  add_subdirectory(qml/Yaap/Ui)

  qt_add_executable(
    yaap_app
    src/app/main.cpp
    src/app/LibraryController.cpp
    src/app/LibraryController.hpp
    src/app/PlayerController.cpp
    src/app/PlayerController.hpp
    src/app/RadioController.cpp
    src/app/RadioController.hpp
    src/app/RadioBrowserDirectoryModel.cpp
    src/app/RadioBrowserDirectoryModel.hpp
  )

  qt_add_qml_module(
    yaap_app
    URI Yaap.App
    VERSION 1.0
    QML_FILES qml/Yaap/App/Main.qml
  )

  set_target_properties(
    yaap_app
    PROPERTIES
      OUTPUT_NAME "Yaap"
      WIN32_EXECUTABLE TRUE
      MACOSX_BUNDLE TRUE
      MACOSX_BUNDLE_BUNDLE_NAME "Yaap"
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.yaap.player"
  )

  target_link_libraries(
    yaap_app
    PRIVATE
      Yaap::Audio
      Yaap::ModRuntime
      Yaap::ProviderRuntime
      Yaap::Services
      yaap_ui
      yaap_uiplugin
      Qt6::Core
      Qt6::Gui
      Qt6::Qml
      Qt6::Quick
      Qt6::QuickControls2
      Qt6::QuickDialogs2
  )
  yaap_enable_warnings(yaap_app)
  add_dependencies(yaap_app yaap_sample_provider)
  if(MSVC)
    # Qt 6.11 headers trigger C4702 under MSVC 19.51; keep it disabled only
    # for the Qt-facing executable while retaining /W4 for project code.
    target_compile_options(yaap_app PRIVATE /wd4702)
  endif()

  if(WIN32)
    if(TARGET Qt6::windeployqt)
      set(YAAP_WINDEPLOYQT_EXECUTABLE "$<TARGET_FILE:Qt6::windeployqt>")
    else()
      get_target_property(_yaap_qmake_executable Qt6::qmake IMPORTED_LOCATION)
      get_filename_component(_yaap_qt_bin_dir "${_yaap_qmake_executable}" DIRECTORY)
      find_program(
        YAAP_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt6
        HINTS "${_yaap_qt_bin_dir}"
        REQUIRED
        NO_DEFAULT_PATH
      )
    endif()

    # PROTOTYPE: Keep this convenient post-build development layout until all
    # local workflows consume the install/CPack layout below; signed releases
    # should be produced only by the release pipeline.
    add_custom_command(
      TARGET yaap_app
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}"
        "-DYAAP_APP_EXECUTABLE=$<TARGET_FILE:yaap_app>"
        "-DYAAP_SOURCE_RUNTIME_DIR=$<TARGET_FILE_DIR:yaap_app>"
        "-DYAAP_DISTRIBUTION_DIR=${YAAP_DISTRIBUTION_ROOT}/$<CONFIG>"
        "-DYAAP_WINDEPLOYQT=${YAAP_WINDEPLOYQT_EXECUTABLE}"
        "-DYAAP_QML_DIR=${CMAKE_CURRENT_SOURCE_DIR}/qml"
        "-DYAAP_SAMPLE_MODS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/samples/mods"
        "-DYAAP_SAMPLE_PROVIDER_EXECUTABLE=$<TARGET_FILE:yaap_sample_provider>"
        "-DYAAP_DEPLOY_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployWindows.cmake"
      COMMENT "Creating runnable Yaap distribution for $<CONFIG>"
      VERBATIM
    )
  endif()

  install(
    TARGETS yaap_app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
  )
  if(WIN32)
    # vcpkg places FFmpeg and its transitive runtime DLLs beside the linked app.
    install(DIRECTORY "$<TARGET_FILE_DIR:yaap_app>/"
      DESTINATION bin FILES_MATCHING PATTERN "*.dll")
  endif()
  qt_generate_deploy_qml_app_script(
    TARGET yaap_app
    OUTPUT_SCRIPT YAAP_QT_DEPLOY_SCRIPT
    NO_UNSUPPORTED_PLATFORM_ERROR
  )
  install(SCRIPT "${YAAP_QT_DEPLOY_SCRIPT}")
endif()

install(DIRECTORY samples/mods/ DESTINATION bin/mods)
if(WIN32)
  install(TARGETS yaap_sample_provider
    RUNTIME DESTINATION bin/mods/org.yaap.sample-provider/bin/windows-x64)
elseif(APPLE)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(_yaap_provider_platform macos-arm64)
  else()
    set(_yaap_provider_platform macos-x64)
  endif()
  install(TARGETS yaap_sample_provider
    RUNTIME DESTINATION "bin/mods/org.yaap.sample-provider/bin/${_yaap_provider_platform}")
else()
  install(TARGETS yaap_sample_provider
    RUNTIME DESTINATION bin/mods/org.yaap.sample-provider/bin/linux-x64)
endif()
install(DIRECTORY docs/ DESTINATION share/Yaap/docs)
install(DIRECTORY sdk/schemas sdk/templates DESTINATION share/Yaap/sdk)
install(FILES THIRD_PARTY_NOTICES.md sbom.spdx.json DESTINATION share/Yaap)

add_custom_target(
  yaap_release_gate
  COMMAND "${CMAKE_COMMAND}"
    "-DYAAP_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DYAAP_REQUIRE_SIGNING=${YAAP_REQUIRE_SIGNING}"
    "-DYAAP_SIGNING_IDENTITY=${YAAP_SIGNING_IDENTITY}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyRelease.cmake"
  VERBATIM
)

set(CPACK_PACKAGE_VENDOR "Yaap")
set(CPACK_PACKAGE_NAME "Yaap")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "Yaap-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md")
if(WIN32)
  set(CPACK_GENERATOR "ZIP" CACHE STRING "CPack package generators")
elseif(APPLE)
  set(CPACK_GENERATOR "DragNDrop" CACHE STRING "CPack package generators")
else()
  set(CPACK_GENERATOR "TGZ" CACHE STRING "CPack package generators")
endif()
include(CPack)

if(BUILD_TESTING)
  find_package(Catch2 3 CONFIG REQUIRED)

  add_executable(
    yaap_tests
    src/app/PlayerController.cpp
    src/app/PlayerController.hpp
    src/app/RadioController.cpp
    src/app/RadioController.hpp
    tests/TestMain.cpp
    tests/FFmpegDecoderTests.cpp
    tests/DomainModelTests.cpp
    tests/ExtensionApiTests.cpp
    tests/LibraryDatabaseTests.cpp
    tests/MiniaudioOutputTests.cpp
    tests/ModRuntimeTests.cpp
    tests/PcmStreamTests.cpp
    tests/PlaybackStateTests.cpp
    tests/PlayerControllerTests.cpp
    tests/ProviderClientTests.cpp
    tests/ProviderPlatformTests.cpp
    tests/ProviderProtocolTests.cpp
    tests/RadioTests.cpp
  )
  target_link_libraries(
    yaap_tests
    PRIVATE
      Yaap::Audio
      Yaap::ModRuntime
      Yaap::ProviderRuntime
      Yaap::ProviderSdk
      Yaap::Services
      Catch2::Catch2
      Qt6::Core
  )
  target_compile_definitions(
    yaap_tests
    PRIVATE
      YAAP_SAMPLE_MODS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/samples/mods"
      YAAP_SAMPLE_PROVIDER_PATH="$<TARGET_FILE:yaap_sample_provider>"
  )
  yaap_enable_warnings(yaap_tests)

  include(Catch)
  add_test(
    NAME "Provider SDK standalone conformance runner"
    COMMAND yaap_provider_conformance
      --executable $<TARGET_FILE:yaap_sample_provider>
      --provider-id org.yaap.sample-provider
  )
  set_tests_properties(
    "Provider SDK standalone conformance runner"
    PROPERTIES
      ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
  )
  catch_discover_tests(
    yaap_tests
    DISCOVERY_MODE PRE_TEST
    DL_PATHS "$<TARGET_FILE_DIR:Qt6::Core>"
    PROPERTIES
      ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        "QT_PLUGIN_PATH=set:$<TARGET_FILE_DIR:Qt6::Core>/../plugins"
  )
endif()
