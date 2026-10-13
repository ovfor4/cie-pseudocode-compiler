if(WIN32)
    set(cpsc_manual_file "${CMAKE_CURRENT_BINARY_DIR}/doc/cpsc.html")
    set(cpsc_manual_format html5)
else()
    set(cpsc_manual_file "${CMAKE_CURRENT_BINARY_DIR}/doc/cpsc.1")
    set(cpsc_manual_format man)
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/GenerateManual.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/GenerateManual.cmake" @ONLY)
set(cpsc_manual_dependencies "${CMAKE_CURRENT_SOURCE_DIR}/doc/cpsc.1.md"
    "${CMAKE_CURRENT_BINARY_DIR}/GenerateManual.cmake"
    "${CPSC_DEPS_MODULE_DIR}/FindDependency.cmake")
if(EXISTS "${CPSC_DEPS_DIR}/packages.sha256")
    list(APPEND cpsc_manual_dependencies "${CPSC_DEPS_DIR}/packages.sha256")
endif()
add_custom_command(
    OUTPUT "${cpsc_manual_file}"
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/GenerateManual.cmake"
    DEPENDS ${cpsc_manual_dependencies}
    COMMENT "Generating cpsc manual (${cpsc_manual_format})"
    VERBATIM
)
add_custom_target(doc DEPENDS "${cpsc_manual_file}")
