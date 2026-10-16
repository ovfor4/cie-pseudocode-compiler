include("${CMAKE_CURRENT_LIST_DIR}/ManualAliases.cmake")
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
if(WIN32)
    foreach(pair IN LISTS CPSC_MANUAL_ALIASES)
        string(REPLACE ":" ";" alias_parts "${pair}")
        list(GET alias_parts 0 alias_name)
        list(GET alias_parts 1 target_name)
        set(alias_target "${CMAKE_CURRENT_BINARY_DIR}/doc/language/keyword/${target_name}.html")
        set(alias_output "${CMAKE_CURRENT_BINARY_DIR}/doc/language/keyword/${alias_name}.html")
        add_custom_command(
            OUTPUT "${alias_output}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${alias_target}" "${alias_output}"
            DEPENDS "${alias_target}"
            COMMENT "Generating language/keyword/${alias_name}.html (alias)"
            VERBATIM
        )
        list(APPEND cpsc_manual_outputs "${alias_output}")
    endforeach()
else()
    foreach(pair IN LISTS CPSC_MANUAL_ALIASES)
        string(REPLACE ":" ";" alias_parts "${pair}")
        list(GET alias_parts 0 alias_name)
        list(GET alias_parts 1 target_name)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/doc/language/keyword/${alias_name}.7" ".so man7/${target_name}.7\n")
        list(APPEND cpsc_manual_outputs "${CMAKE_CURRENT_BINARY_DIR}/doc/language/keyword/${alias_name}.7")
    endforeach()
endif()
add_custom_target(doc DEPENDS ${cpsc_manual_outputs})
