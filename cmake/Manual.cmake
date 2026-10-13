if(WIN32)
    set(cpsc_manual_format html5)
else()
    set(cpsc_manual_format man)
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/GenerateManual.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/GenerateManual.cmake" @ONLY)
file(GLOB_RECURSE cpsc_language_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/doc/language/cpsc-*.7.md")
set(cpsc_manual_sources "${CMAKE_CURRENT_SOURCE_DIR}/doc/cpsc.1.md"
    ${cpsc_language_sources})
set(cpsc_manual_dependencies
    "${CMAKE_CURRENT_BINARY_DIR}/GenerateManual.cmake"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ManualLinks.lua"
    "${CPSC_DEPS_MODULE_DIR}/FindDependency.cmake")
if(EXISTS "${CPSC_DEPS_DIR}/packages.sha256")
    list(APPEND cpsc_manual_dependencies "${CPSC_DEPS_DIR}/packages.sha256")
endif()
set(cpsc_manual_outputs)
foreach(source IN LISTS cpsc_manual_sources)
    file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}/doc" "${source}")
    if(WIN32)
        string(REGEX REPLACE "\\.[17]\\.md$" ".html" relative "${relative}")
    else()
        string(REGEX REPLACE "\\.md$" "" relative "${relative}")
    endif()
    set(output "${CMAKE_CURRENT_BINARY_DIR}/doc/${relative}")
    add_custom_command(
        OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" "-DCPSC_MANUAL_SOURCE=${source}"
            "-DCPSC_MANUAL_OUTPUT=${output}"
            -P "${CMAKE_CURRENT_BINARY_DIR}/GenerateManual.cmake"
        DEPENDS "${source}" ${cpsc_manual_dependencies}
        COMMENT "Generating ${relative} (${cpsc_manual_format})"
        VERBATIM
    )
    list(APPEND cpsc_manual_outputs "${output}")
endforeach()
add_custom_target(doc DEPENDS ${cpsc_manual_outputs})
