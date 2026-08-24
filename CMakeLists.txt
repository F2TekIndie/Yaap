cmake_minimum_required(VERSION 4.2)

project(
  Yaap
  VERSION 0.1.0
  DESCRIPTION "Cross-platform moddable music player"
  LANGUAGES CXX
)

option(YAAP_BUILD_APP "Build the Qt Quick desktop application" ON)

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
  src/core/PlaybackState.cpp
  src/core/PlaybackState.hpp
)
add_library(Yaap::Core ALIAS yaap_core)
target_include_directories(yaap_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(yaap_core PUBLIC cxx_std_20)
yaap_enable_warnings(yaap_core)

include(CTest)

if(YAAP_BUILD_APP OR BUILD_TESTING)
  find_package(FFmpeg REQUIRED)
  find_path(MINIAUDIO_INCLUDE_DIR NAMES miniaudio.h REQUIRED)

  add_library(
    yaap_audio
    src/audio/AudioBuffer.cpp
    src/audio/AudioBuffer.hpp
    src/audio/DecodedAudio.hpp
    src/audio/FFmpegDecoder.cpp
    src/audio/FFmpegDecoder.hpp
    src/audio/MiniaudioOutput.cpp
    src/audio/MiniaudioOutput.hpp
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
endif()

if(YAAP_BUILD_APP)
  find_package(
    Qt6 6.5
    REQUIRED
    COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2
  )

  qt_standard_project_setup(REQUIRES 6.5)
  qt_policy(SET QTP0004 NEW)

  qt_add_executable(
    yaap_app
    src/app/main.cpp
    src/app/PlayerController.cpp
    src/app/PlayerController.hpp
  )

  qt_add_qml_module(
    yaap_app
    URI Yaap
    VERSION 1.0
    QML_FILES qml/Main.qml
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
      Qt6::Core
      Qt6::Gui
      Qt6::Qml
      Qt6::Quick
      Qt6::QuickControls2
      Qt6::QuickDialogs2
  )
  yaap_enable_warnings(yaap_app)
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

    # PROTOTYPE: This produces a directly runnable development distribution.
    # Replace it with install rules plus CPack, signing, and clean-machine
    # release validation when the product packaging pipeline is introduced.
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
endif()

if(BUILD_TESTING)
  find_package(Catch2 3 CONFIG REQUIRED)

  add_executable(
    yaap_tests
    tests/AudioBufferTests.cpp
    tests/FFmpegDecoderTests.cpp
    tests/MiniaudioOutputTests.cpp
    tests/PlaybackStateTests.cpp
  )
  target_link_libraries(
    yaap_tests
    PRIVATE Yaap::Audio Catch2::Catch2WithMain
  )
  yaap_enable_warnings(yaap_tests)

  include(Catch)
  catch_discover_tests(yaap_tests)
endif()
