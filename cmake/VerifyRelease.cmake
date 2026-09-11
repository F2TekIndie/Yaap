foreach(_required_file IN ITEMS
    "${YAAP_SOURCE_DIR}/LICENSE"
    "${YAAP_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    "${YAAP_SOURCE_DIR}/sbom.spdx.json"
    "${YAAP_SOURCE_DIR}/docs/theme-format-policy.md"
    "${YAAP_SOURCE_DIR}/docs/schemas/theme-manifest-v1.schema.json")
  if(NOT EXISTS "${_required_file}")
    message(FATAL_ERROR "Release input is missing: ${_required_file}")
  endif()
endforeach()

if(YAAP_REQUIRE_SIGNING AND "${YAAP_SIGNING_IDENTITY}" STREQUAL "")
  message(FATAL_ERROR
    "YAAP_REQUIRE_SIGNING is enabled but YAAP_SIGNING_IDENTITY is empty. "
    "Provide a Windows certificate thumbprint or Apple signing identity.")
endif()

message(STATUS "Yaap release metadata gate passed")
