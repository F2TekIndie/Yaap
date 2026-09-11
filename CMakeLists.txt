cmake_minimum_required(VERSION 4.2)

project(
  Yaap
  VERSION 0.2.0
  DESCRIPTION "Cross-platform moddable music player"
  LANGUAGES CXX
)

option(YAAP_BUILD_APP "Build the Qt Quick desktop application" ON)
option(YAAP_ENABLE_SANITIZERS "Enable supported address/undefined sanitizers" OFF)
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

# Migrate build trees configured before the distribution moved out of the build
# directory, while preserving an explicitly customized destination.
if(DEFINED YAAP_DISTRIBUTION_ROOT
    AND YAAP_DISTRIBUTION_ROOT STREQUAL "${CMAKE_BINARY_DIR}/distribution")
  unset(YAAP_DISTRIBUTION_ROOT CACHE)
endif()
set(
  YAAP_DISTRIBUTION_ROOT
  "${PROJECT_SOURCE_DIR}/distribution"
  CACHE PATH
  "Directory where the runnable Release application layout is created"
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
  find_package(Qt6 6.5 REQUIRED COMPONENTS Core Concurrent Gui Network Qml Sql Svg)

  add_library(
    yaap_extension_api
    src/extension_api/ApiVersion.cpp
    src/extension_api/ApiVersion.hpp
    src/extension_api/ModManifest.cpp
    src/extension_api/ModManifest.hpp
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
    yaap_mod_runtime
    src/mods/ModApiInfo.cpp
    src/mods/ModApiInfo.hpp
    src/mods/ModManager.cpp
    src/mods/ModManager.hpp
    src/mods/ModManifestParser.cpp
    src/mods/ModManifestParser.hpp
    src/mods/ThemeManager.cpp
    src/mods/ThemeManager.hpp
  )
  add_library(Yaap::ModRuntime ALIAS yaap_mod_runtime)
  target_include_directories(yaap_mod_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_link_libraries(
    yaap_mod_runtime
    PUBLIC Yaap::ExtensionApi Qt6::Core Qt6::Gui Qt6::Svg
  )
  yaap_enable_warnings(yaap_mod_runtime)

  add_library(
    yaap_audio
    src/audio/AudioAnalysisEngine.cpp
    src/audio/AudioAnalysisEngine.hpp
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
    src/provider_runtime/ProviderCache.cpp
    src/provider_runtime/ProviderCache.hpp
    src/radio/RadioPlaylist.cpp
    src/radio/RadioPlaylist.hpp
    src/radio/RadioPlaylistLoader.cpp
    src/radio/RadioPlaylistLoader.hpp
    src/radio/RadioBrowserClient.cpp
    src/radio/RadioBrowserClient.hpp
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
endif()

if(YAAP_BUILD_APP)
  find_package(
    Qt6 6.5
    REQUIRED
    COMPONENTS Core Gui Widgets Qml Quick QuickControls2 QuickDialogs2
  )

  qt_standard_project_setup(REQUIRES 6.5)
  qt_policy(SET QTP0004 NEW)

  add_subdirectory(qml/Yaap/ModApi)
  add_subdirectory(qml/Yaap/Ui)

  qt_add_executable(
    yaap_app
    src/app/main.cpp
    src/app/ApplicationSession.cpp
    src/app/ApplicationSession.hpp
    src/app/TrayController.cpp
    src/app/TrayController.hpp
    src/app/AudioVisualizationModel.cpp
    src/app/AudioVisualizationModel.hpp
    src/app/LibraryController.cpp
    src/app/LibraryController.hpp
    src/app/PlayerController.cpp
    src/app/PlayerController.hpp
    src/app/RadioController.cpp
    src/app/RadioController.hpp
    src/app/RadioBrowserDirectoryModel.cpp
    src/app/RadioBrowserDirectoryModel.hpp
    src/app/WindowShapeController.cpp
    src/app/WindowShapeController.hpp
    src/app/WindowPresentationController.cpp
    src/app/WindowPresentationController.hpp
  )

  qt_add_resources(yaap_app "yaap_tray_assets" PREFIX "/yaap"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux"
    FILES packaging/linux/org.yaap.Yaap.svg)
  target_link_libraries(yaap_app PRIVATE Qt6::Widgets)

  qt_add_qml_module(
    yaap_app
    URI Yaap.App
    VERSION 1.0
    QML_FILES
      qml/Yaap/App/Main.qml
      qml/Yaap/App/WindowChrome.qml
      qml/Yaap/App/SettingsContent.qml
      qml/Yaap/App/FullPlayerView.qml
      qml/Yaap/App/MiniPlayerView.qml
      qml/Yaap/App/TransportButton.qml
    RESOURCES
      qml/Yaap/App/icons/library.svg
      qml/Yaap/App/icons/radio.svg
      qml/Yaap/App/icons/open-file.svg
      qml/Yaap/App/icons/open-stream.svg
      qml/Yaap/App/icons/settings.svg
      qml/Yaap/App/icons/miniplayer.svg
      qml/Yaap/App/icons/mute.svg
      qml/Yaap/App/icons/pause.svg
      qml/Yaap/App/icons/play.svg
      qml/Yaap/App/icons/restore.svg
      qml/Yaap/App/icons/stop.svg
      qml/Yaap/App/icons/volume.svg
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

  if(UNIX AND NOT APPLE)
    # Keep the executable separate from the generated Yaap/ QML module directory.
    set_target_properties(yaap_app PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
  endif()

  target_link_libraries(
    yaap_app
    PRIVATE
      Yaap::Audio
      Yaap::ModRuntime
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
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 6.5 REQUIRED COMPONENTS DBus)
    target_sources(yaap_app PRIVATE src/app/MprisService.cpp src/app/MprisService.hpp)
    target_link_libraries(yaap_app PRIVATE Qt6::DBus)
    target_compile_definitions(yaap_app PRIVATE YAAP_HAVE_MPRIS)
    install(FILES packaging/linux/org.yaap.Yaap.desktop DESTINATION share/applications)
    install(FILES packaging/linux/org.yaap.Yaap.svg DESTINATION share/icons/hicolor/scalable/apps)
  endif()
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

    # PROTOTYPE: Keep this convenient runnable Release layout until the signed
    # install/CPack pipeline becomes the sole release path. The deployment
    # script intentionally ignores non-Release configurations.
    add_custom_command(
      TARGET yaap_app
      POST_BUILD
      COMMAND
        "${CMAKE_COMMAND}"
        "-DYAAP_APP_EXECUTABLE=$<TARGET_FILE:yaap_app>"
        "-DYAAP_SOURCE_RUNTIME_DIR=$<TARGET_FILE_DIR:yaap_app>"
        "-DYAAP_DISTRIBUTION_DIR=${YAAP_DISTRIBUTION_ROOT}"
        "-DYAAP_WINDEPLOYQT=${YAAP_WINDEPLOYQT_EXECUTABLE}"
        "-DYAAP_QML_DIR=${CMAKE_CURRENT_SOURCE_DIR}/qml"
        "-DYAAP_SAMPLE_MODS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/samples/mods"
        "-DYAAP_DEPLOY_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployWindows.cmake"
      COMMENT "Running Release-only Yaap distribution step"
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
  set(_yaap_deploy_plugin_options)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND Qt6_VERSION VERSION_GREATER_EQUAL 6.10)
    # Qt's default deployment may select only xcb. Include the native Wayland
    # backend for niri and offscreen for isolated startup/lifecycle checks.
    list(APPEND _yaap_deploy_plugin_options INCLUDE_PLUGINS qwayland qwayland-egl qoffscreen
      INCLUDE_PLUGIN_TYPES wayland-shell-integration wayland-graphics-integration-client)
  endif()
  qt_generate_deploy_qml_app_script(
    TARGET yaap_app
    OUTPUT_SCRIPT YAAP_QT_DEPLOY_SCRIPT
    NO_UNSUPPORTED_PLATFORM_ERROR
    ${_yaap_deploy_plugin_options}
  )
  install(SCRIPT "${YAAP_QT_DEPLOY_SCRIPT}")

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_target(yaap_distribution ALL
      COMMAND "${CMAKE_COMMAND}"
        "-DYAAP_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DYAAP_DISTRIBUTION_DIR=${YAAP_DISTRIBUTION_ROOT}/linux"
        "-DYAAP_DEPLOY_CONFIGURATION=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployLinux.cmake"
      DEPENDS yaap_app
      COMMENT "Running Release-only Linux distribution step"
      VERBATIM)
  endif()
endif()

install(DIRECTORY samples/mods/ DESTINATION bin/mods
  PATTERN "org.yaap.sample-provider" EXCLUDE)
install(DIRECTORY docs/ DESTINATION share/Yaap/docs)
install(FILES LICENSE THIRD_PARTY_NOTICES.md sbom.spdx.json DESTINATION share/Yaap)

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
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
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
  if(YAAP_BUILD_APP)
    add_executable(yaap_settings_ui_smoke tests/SettingsUiSmoke.cpp
      src/app/TrayController.cpp src/app/TrayController.hpp
      src/app/ApplicationSession.cpp src/app/ApplicationSession.hpp)
    target_link_libraries(yaap_settings_ui_smoke PRIVATE Yaap::ModRuntime
      Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::Widgets)
    target_compile_definitions(yaap_settings_ui_smoke PRIVATE
      YAAP_SAMPLE_MODS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/samples/mods"
      YAAP_SETTINGS_QML="${CMAKE_CURRENT_SOURCE_DIR}/qml/Yaap/App/SettingsContent.qml")
    add_test(NAME "Settings UI interactions" COMMAND yaap_settings_ui_smoke)
    set_tests_properties("Settings UI interactions" PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_QPA_PLATFORMTHEME=generic"
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>;QT_PLUGIN_PATH=set:$<TARGET_FILE_DIR:Qt6::Core>/../plugins")
  endif()

  add_executable(
    yaap_tests
    src/app/AudioVisualizationModel.cpp
    src/app/AudioVisualizationModel.hpp
    src/app/LibraryController.cpp
    src/app/LibraryController.hpp
    src/app/PlayerController.cpp
    src/app/PlayerController.hpp
    src/app/RadioController.cpp
    src/app/RadioController.hpp
    src/app/WindowPresentationController.cpp
    src/app/WindowPresentationController.hpp
    tests/TestMain.cpp
    tests/AudioAnalysisTests.cpp
    tests/FFmpegDecoderTests.cpp
    tests/DomainModelTests.cpp
    tests/ExtensionApiTests.cpp
    tests/LibraryDatabaseTests.cpp
    tests/MiniaudioOutputTests.cpp
    tests/ModRuntimeTests.cpp
    tests/ThemeCustomizationTests.cpp
    tests/PcmStreamTests.cpp
    tests/PlaybackStateTests.cpp
    tests/PlayerControllerTests.cpp
    tests/RadioTests.cpp
    tests/CacheTests.cpp
    tests/WindowPresentationControllerTests.cpp
  )
  target_link_libraries(
    yaap_tests
    PRIVATE
      Yaap::Audio
      Yaap::ModRuntime
      Yaap::Services
      Catch2::Catch2
      Qt6::Core
  )
  target_compile_definitions(
    yaap_tests
    PRIVATE
      YAAP_SAMPLE_MODS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/samples/mods"
  )
  yaap_enable_warnings(yaap_tests)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt6 6.5 REQUIRED COMPONENTS DBus)
    target_sources(yaap_tests PRIVATE src/app/MprisService.cpp src/app/MprisService.hpp
      src/app/ApplicationSession.cpp src/app/ApplicationSession.hpp tests/MprisTests.cpp tests/DmsThemeTests.cpp)
    target_link_libraries(yaap_tests PRIVATE Qt6::DBus)
  endif()

  include(Catch)
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
