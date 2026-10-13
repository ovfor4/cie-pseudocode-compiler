# Read HEAD at packaging time so subsequent commits do not require reconfiguration.
find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${CPSC_SOURCE_DIR}" log -1 "--format=%ct:%h"
    OUTPUT_VARIABLE cpsc_revision
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE cpsc_git_result
)
if(NOT cpsc_git_result STREQUAL "0" OR NOT cpsc_revision MATCHES "^([0-9]+):([0-9a-f]+)$")
    message(FATAL_ERROR "Cannot read the HEAD commit for the release version")
endif()
set(ENV{SOURCE_DATE_EPOCH} "${CMAKE_MATCH_1}")
set(cpsc_short_id "${CMAKE_MATCH_2}")
string(TIMESTAMP cpsc_commit_time "%Y-%m-%d-%H-%M-%S" UTC)
set(cpsc_package_name "cpsc-${cpsc_commit_time}-${cpsc_short_id}-${CPSC_PLATFORM}")
set(cpsc_package_dir "${CPSC_BINARY_DIR}/release/${cpsc_package_name}")
if(CPSC_WINDOWS)
    set(cpsc_archive "${CPSC_BINARY_DIR}/${cpsc_package_name}.zip")
    set(cpsc_binary_name cpsc.exe)
    set(cpsc_installer install.ps1)
    set(cpsc_manual cpsc.html)
    set(cpsc_tar_flags cf)
    set(cpsc_archive_format zip)
else()
    set(cpsc_archive "${CPSC_BINARY_DIR}/${cpsc_package_name}.tar.xz")
    set(cpsc_binary_name cpsc)
    set(cpsc_installer install.sh)
    set(cpsc_manual cpsc.1)
    set(cpsc_tar_flags cJf)
    set(cpsc_archive_format gnutar)
endif()

file(REMOVE_RECURSE "${cpsc_package_dir}")
file(MAKE_DIRECTORY "${cpsc_package_dir}/doc")
configure_file("${CPSC_EXECUTABLE}" "${cpsc_package_dir}/${cpsc_binary_name}" COPYONLY)
file(COPY "${CPSC_SOURCE_DIR}/${cpsc_installer}" "${CPSC_SOURCE_DIR}/LICENSE"
    DESTINATION "${cpsc_package_dir}")
file(COPY "${CPSC_BINARY_DIR}/doc/${cpsc_manual}" DESTINATION "${cpsc_package_dir}/doc")
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
