foreach(_required_variable IN ITEMS
    YAAP_APP_EXECUTABLE
    YAAP_SOURCE_RUNTIME_DIR
    YAAP_DISTRIBUTION_DIR
    YAAP_WINDEPLOYQT
    YAAP_QML_DIR
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

# The per-configuration directory is intentionally rebuilt from scratch so a
# removed dependency cannot remain hidden by a stale DLL or Qt plugin.
file(REMOVE_RECURSE "${YAAP_DISTRIBUTION_DIR}")
file(MAKE_DIRECTORY "${YAAP_DISTRIBUTION_DIR}")

get_filename_component(_app_filename "${YAAP_APP_EXECUTABLE}" NAME)
set(_deployed_app "${YAAP_DISTRIBUTION_DIR}/${_app_filename}")
file(COPY_FILE "${YAAP_APP_EXECUTABLE}" "${_deployed_app}" ONLY_IF_DIFFERENT)

# vcpkg's app-local deployment places FFmpeg and its transitive runtime DLLs
# beside the freshly linked executable before this post-build command runs.
file(GLOB _runtime_dlls LIST_DIRECTORIES FALSE "${YAAP_SOURCE_RUNTIME_DIR}/*.dll")
if(_runtime_dlls)
  file(COPY ${_runtime_dlls} DESTINATION "${YAAP_DISTRIBUTION_DIR}")
endif()

if(YAAP_DEPLOY_CONFIGURATION STREQUAL "Debug")
  set(_qt_configuration_argument --debug)
else()
  set(_qt_configuration_argument --release)
endif()

execute_process(
  COMMAND
    "${YAAP_WINDEPLOYQT}"
    "${_qt_configuration_argument}"
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
