include(FindPackageHandleStandardArgs)

set(_ffmpeg_components avformat avcodec avutil swresample)

find_path(
  FFMPEG_INCLUDE_DIR
  NAMES libavformat/avformat.h
  PATH_SUFFIXES include
)

foreach(_component IN LISTS _ffmpeg_components)
  string(TOUPPER "${_component}" _component_upper)
  find_library(
    FFMPEG_${_component_upper}_LIBRARY
    NAMES "${_component}"
    PATH_SUFFIXES lib
  )
  list(APPEND _ffmpeg_required_vars FFMPEG_${_component_upper}_LIBRARY)
endforeach()

find_package_handle_standard_args(
  FFmpeg
  REQUIRED_VARS FFMPEG_INCLUDE_DIR ${_ffmpeg_required_vars}
)

if(FFmpeg_FOUND)
  foreach(_component IN LISTS _ffmpeg_components)
    string(TOUPPER "${_component}" _component_upper)
    if(NOT TARGET FFmpeg::${_component})
      add_library(FFmpeg::${_component} UNKNOWN IMPORTED)
      set_target_properties(
        FFmpeg::${_component}
        PROPERTIES
          IMPORTED_LOCATION "${FFMPEG_${_component_upper}_LIBRARY}"
          INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
      )
    endif()
  endforeach()
endif()

mark_as_advanced(
  FFMPEG_INCLUDE_DIR
  FFMPEG_AVFORMAT_LIBRARY
  FFMPEG_AVCODEC_LIBRARY
  FFMPEG_AVUTIL_LIBRARY
  FFMPEG_SWRESAMPLE_LIBRARY
)

