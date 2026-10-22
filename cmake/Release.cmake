# Read the last compiler commit at packaging time so subsequent commits do not require reconfiguration.
include("${CPSC_SOURCE_DIR}/cmake/FindDependency.cmake")
include("${CPSC_SOURCE_DIR}/cmake/ManualAliases.cmake")
cpsc_find_dependency(PROGRAM GIT_EXECUTABLE NAMES git REQUIRED)
find_package(Git REQUIRED)
file(STRINGS "${CPSC_SOURCE_DIR}/cmake/CompilerPaths.txt" cpsc_compiler_paths)
list(TRANSFORM cpsc_compiler_paths STRIP)
list(FILTER cpsc_compiler_paths EXCLUDE REGEX "^$")
execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${CPSC_SOURCE_DIR}" log -1 "--format=%ct:%h" -- ${cpsc_compiler_paths}
    OUTPUT_VARIABLE cpsc_revision
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE cpsc_git_result
)
if(NOT cpsc_git_result STREQUAL "0" OR NOT cpsc_revision MATCHES "^([0-9]+):([0-9a-f]+)$")
    message(FATAL_ERROR "Cannot read the last compiler commit for the release version")
endif()
set(ENV{SOURCE_DATE_EPOCH} "${CMAKE_MATCH_1}")
set(cpsc_short_id "${CMAKE_MATCH_2}")
string(TIMESTAMP cpsc_commit_time "%Y%m%d-%H%M%S" UTC)
set(cpsc_package_name "cpsc-${cpsc_commit_time}-${cpsc_short_id}-${CPSC_PLATFORM}")
set(cpsc_package_dir "${CPSC_BINARY_DIR}/release/${cpsc_package_name}")
if(CPSC_WINDOWS)
    set(cpsc_archive "${CPSC_BINARY_DIR}/${cpsc_package_name}.zip")
    set(cpsc_binary_name cpsc.exe)
    set(cpsc_manual cpsc.html)
    set(cpsc_language_extension html)
    set(cpsc_tar_flags cf)
    set(cpsc_archive_format zip)
else()
    set(cpsc_archive "${CPSC_BINARY_DIR}/${cpsc_package_name}.tar.xz")
    set(cpsc_binary_name cpsc)
    set(cpsc_manual cpsc.1)
    set(cpsc_language_extension 7)
    set(cpsc_tar_flags cJf)
    set(cpsc_archive_format gnutar)
endif()

file(REMOVE_RECURSE "${cpsc_package_dir}")
file(MAKE_DIRECTORY "${cpsc_package_dir}/doc")
configure_file("${CPSC_EXECUTABLE}" "${cpsc_package_dir}/${cpsc_binary_name}" COPYONLY)
file(COPY "${CPSC_SOURCE_DIR}/LICENSE" DESTINATION "${cpsc_package_dir}")
if(CPSC_WINDOWS)
    # cmd needs CRLF line endings whatever the packaging host is; install.cmd contains no @NAME@ sequences.
    configure_file("${CPSC_SOURCE_DIR}/install.cmd" "${cpsc_package_dir}/install.cmd"
        @ONLY NEWLINE_STYLE CRLF)
else()
    file(COPY "${CPSC_SOURCE_DIR}/install.sh" DESTINATION "${cpsc_package_dir}")
endif()
file(COPY "${CPSC_BINARY_DIR}/doc/${cpsc_manual}" DESTINATION "${cpsc_package_dir}/doc")
# Package the current source set, preserving categories and ignoring stale outputs.
file(GLOB_RECURSE cpsc_language_sources "${CPSC_SOURCE_DIR}/doc/language/cpsc-*.7.md")
if(NOT cpsc_language_sources)
    message(FATAL_ERROR "Language manual sources are missing")
endif()
foreach(source IN LISTS cpsc_language_sources)
    file(RELATIVE_PATH relative "${CPSC_SOURCE_DIR}/doc/language" "${source}")
    string(REGEX REPLACE "\\.7\\.md$" ".${cpsc_language_extension}" relative "${relative}")
    get_filename_component(directory "${relative}" DIRECTORY)
    file(COPY "${CPSC_BINARY_DIR}/doc/language/${relative}"
        DESTINATION "${cpsc_package_dir}/doc/language/${directory}")
endforeach()
foreach(pair IN LISTS CPSC_MANUAL_ALIASES)
    string(REPLACE ":" ";" alias_parts "${pair}")
    list(GET alias_parts 0 alias_name)
    set(relative "keyword/${alias_name}.${cpsc_language_extension}")
    get_filename_component(directory "${relative}" DIRECTORY)
    if(NOT EXISTS "${CPSC_BINARY_DIR}/doc/language/${relative}")
        message(FATAL_ERROR "Language manual alias is missing: ${relative}")
    endif()
    file(MAKE_DIRECTORY "${cpsc_package_dir}/doc/language/${directory}")
    file(COPY "${CPSC_BINARY_DIR}/doc/language/${relative}"
        DESTINATION "${cpsc_package_dir}/doc/language/${directory}")
endforeach()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar "${cpsc_tar_flags}" "${cpsc_archive}"
        "--format=${cpsc_archive_format}" "${cpsc_package_name}"
    WORKING_DIRECTORY "${CPSC_BINARY_DIR}/release"
    RESULT_VARIABLE cpsc_tar_result
)
if(NOT cpsc_tar_result STREQUAL "0")
    file(REMOVE "${cpsc_archive}")
    message(FATAL_ERROR "Cannot create the release archive (${cpsc_tar_result})")
endif()
message(STATUS "Release archive: ${cpsc_archive}")
