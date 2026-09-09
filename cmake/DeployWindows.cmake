if(DEFINED YAAP_DEPLOY_CONFIGURATION
    AND NOT YAAP_DEPLOY_CONFIGURATION STREQUAL "Release")
  message(STATUS
    "Skipping runnable distribution for ${YAAP_DEPLOY_CONFIGURATION}; Release only")
  return()
endif()

foreach(_required_variable IN ITEMS
    YAAP_APP_EXECUTABLE
    YAAP_SOURCE_RUNTIME_DIR
    YAAP_DISTRIBUTION_DIR
    YAAP_WINDEPLOYQT
    YAAP_QML_DIR
    YAAP_SAMPLE_MODS_DIR
    YAAP_SAMPLE_PROVIDER_EXECUTABLE
    YAAP_DEPLOY_CONFIGURATION)
  if(NOT DEFINED ${_required_variable} OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "DeployWindows.cmake requires ${_required_variable}")
  endif()
endforeach()

if(NOT EXISTS "${YAAP_APP_EXECUTABLE}")
  message(FATAL_ERROR "Yaap executable does not exist: ${YAAP_APP_EXECUTABLE}")
endif()

if(NOT EXISTS "${YAAP_WINDEPLOYQT}")
  message(FATAL_ERROR "windeployqt does not exist: ${YAAP_WINDEPLOYQT}")
endif()

# Refresh Windows files while preserving the independent Linux distribution.
file(GLOB _old_entries LIST_DIRECTORIES TRUE "${YAAP_DISTRIBUTION_DIR}/*")
foreach(_entry IN LISTS _old_entries)
  get_filename_component(_name "${_entry}" NAME)
  if(NOT _name STREQUAL "linux")
    file(REMOVE_RECURSE "${_entry}")
  endif()
endforeach()
file(MAKE_DIRECTORY "${YAAP_DISTRIBUTION_DIR}")

get_filename_component(_app_filename "${YAAP_APP_EXECUTABLE}" NAME)
set(_deployed_app "${YAAP_DISTRIBUTION_DIR}/${_app_filename}")
file(COPY_FILE "${YAAP_APP_EXECUTABLE}" "${_deployed_app}" ONLY_IF_DIFFERENT)

file(COPY "${YAAP_SAMPLE_MODS_DIR}/" DESTINATION "${YAAP_DISTRIBUTION_DIR}/mods")
file(MAKE_DIRECTORY
  "${YAAP_DISTRIBUTION_DIR}/mods/org.yaap.sample-provider/bin/windows-x64")
file(COPY_FILE
  "${YAAP_SAMPLE_PROVIDER_EXECUTABLE}"
  "${YAAP_DISTRIBUTION_DIR}/mods/org.yaap.sample-provider/bin/windows-x64/YaapSampleProvider.exe"
  ONLY_IF_DIFFERENT)

# vcpkg's app-local deployment places FFmpeg and its transitive runtime DLLs
# beside the freshly linked executable before this post-build command runs.
file(GLOB _runtime_dlls LIST_DIRECTORIES FALSE "${YAAP_SOURCE_RUNTIME_DIR}/*.dll")
if(_runtime_dlls)
  file(COPY ${_runtime_dlls} DESTINATION "${YAAP_DISTRIBUTION_DIR}")
endif()

execute_process(
  COMMAND
    "${YAAP_WINDEPLOYQT}"
    --release
    --dir "${YAAP_DISTRIBUTION_DIR}"
    --qmldir "${YAAP_QML_DIR}"
    --verbose 1
    "${_deployed_app}"
  RESULT_VARIABLE _deploy_result
  OUTPUT_VARIABLE _deploy_output
  ERROR_VARIABLE _deploy_error
)

if(NOT _deploy_result EQUAL 0)
  message(
    FATAL_ERROR
    "windeployqt failed with exit code ${_deploy_result}\n"
    "stdout:\n${_deploy_output}\n"
    "stderr:\n${_deploy_error}"
  )
endif()

message(STATUS "Runnable Yaap distribution: ${YAAP_DISTRIBUTION_DIR}")
