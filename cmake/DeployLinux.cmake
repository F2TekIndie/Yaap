if(DEFINED YAAP_DEPLOY_CONFIGURATION
    AND NOT YAAP_DEPLOY_CONFIGURATION STREQUAL "Release")
  message(STATUS "Skipping Linux distribution for ${YAAP_DEPLOY_CONFIGURATION}; Release only")
  return()
endif()

foreach(_variable IN ITEMS YAAP_BUILD_DIR YAAP_DISTRIBUTION_DIR YAAP_DEPLOY_CONFIGURATION)
  if(NOT DEFINED ${_variable} OR "${${_variable}}" STREQUAL "")
    message(FATAL_ERROR "DeployLinux.cmake requires ${_variable}")
  endif()
endforeach()
get_filename_component(_destination_name "${YAAP_DISTRIBUTION_DIR}" NAME)
if(NOT _destination_name STREQUAL "linux")
  message(FATAL_ERROR "Linux distribution destination must end in /linux")
endif()

# Stage first so a failed Qt deployment leaves the previous distribution usable.
set(_stage "${YAAP_BUILD_DIR}/linux-distribution-stage")
file(REMOVE_RECURSE "${_stage}")
if(EXISTS "${_stage}")
  message(FATAL_ERROR "Could not clear Linux staging directory: ${_stage}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${YAAP_BUILD_DIR}"
    --config Release --prefix "${_stage}"
  RESULT_VARIABLE _result)
if(NOT _result EQUAL 0)
  message(FATAL_ERROR "Linux distribution install failed: ${_result}")
endif()

# The launcher supplies the application's bundled libraries.
file(WRITE "${_stage}/Yaap" [=[#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
export LD_LIBRARY_PATH="$root/lib64:$root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$root/bin/Yaap" "$@"
]=])
file(CHMOD "${_stage}/Yaap" PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
if(EXISTS "${YAAP_DISTRIBUTION_DIR}")
  # FUSE retains open libraries as hidden files while a previous player runs.
  # Move the old layout aside so those files cannot contaminate the new one.
  string(TIMESTAMP _stamp "%Y%m%d%H%M%S")
  set(_previous "${YAAP_DISTRIBUTION_DIR}.previous-${_stamp}")
  file(RENAME "${YAAP_DISTRIBUTION_DIR}" "${_previous}" RESULT _rename_result)
  if(NOT _rename_result STREQUAL "0")
    message(FATAL_ERROR "Could not move previous Linux distribution: ${_rename_result}")
  endif()
  file(REMOVE_RECURSE "${_previous}")
endif()
file(MAKE_DIRECTORY "${YAAP_DISTRIBUTION_DIR}")
file(COPY "${_stage}/" DESTINATION "${YAAP_DISTRIBUTION_DIR}")
message(STATUS "Runnable Linux distribution: ${YAAP_DISTRIBUTION_DIR}")
