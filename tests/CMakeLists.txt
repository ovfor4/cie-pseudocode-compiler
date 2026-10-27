cpsc_find_dependency(PROGRAM PERL_EXECUTABLE NAMES perl REQUIRED)
find_package(Perl REQUIRED)
set(CPSC_TEST_SEED "" CACHE STRING "Replay seed (defaults to HEAD at test execution)")
set(CPSC_TEST_COUNT 10000 CACHE STRING "Random records per differential group")

add_test(NAME harness COMMAND "${PERL_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_runner.pl")
add_test(NAME generators COMMAND "${PERL_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/check_generators.pl" "${PERL_EXECUTABLE}")
foreach(suite i ii iii)
    file(GLOB cases CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${suite}/*/case.json")
    foreach(manifest IN LISTS cases)
        get_filename_component(case_dir "${manifest}" DIRECTORY)
        get_filename_component(case_name "${case_dir}" NAME)
        set(reference_args)
        if(suite STREQUAL "ii")
            set(reference_target "cpsc-reference-${case_name}")
            add_executable(${reference_target} "${case_dir}/reference.cc")
            target_compile_features(${reference_target} PRIVATE cxx_std_17)
            list(APPEND reference_args --reference "$<TARGET_FILE:${reference_target}>")
        endif()
        add_test(NAME "${suite}/${case_name}" COMMAND "${PERL_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/run.pl"
            --case "${manifest}" --cpsc "$<TARGET_FILE:cpsc>"
            --run-mode "${CPSC_RUN_MODE}" --perl "${PERL_EXECUTABLE}"
            --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}"
            --seed "${CPSC_TEST_SEED}" --count "${CPSC_TEST_COUNT}"
            --results "${CMAKE_BINARY_DIR}/test-results" ${reference_args})
        set_tests_properties("${suite}/${case_name}" PROPERTIES
            LABELS "${suite}" TIMEOUT 900)
    endforeach()
endforeach()

# Keep the libraries beside their built objects without assuming a build directory name.
foreach(library exp-string exp-memory)
    set(cpsc_library_case "${CMAKE_BINARY_DIR}/test-cases/i/${library}")
    foreach(file program.cps case.json expected.txt expected-stderr.txt)
        configure_file("library/${library}/${file}" "${cpsc_library_case}/${file}" COPYONLY)
    endforeach()
    add_test(NAME "i/${library}" COMMAND "${PERL_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/run.pl"
        --case "${cpsc_library_case}/case.json" --cpsc "$<TARGET_FILE:cpsc>"
        --run-mode "${CPSC_RUN_MODE}" --perl "${PERL_EXECUTABLE}"
        --cc "${CMAKE_C_COMPILER}" --cxx "${CMAKE_CXX_COMPILER}"
        --seed "${CPSC_TEST_SEED}" --count "${CPSC_TEST_COUNT}"
        --results "${CMAKE_BINARY_DIR}/test-results")
    set_tests_properties("i/${library}" PROPERTIES LABELS "i" TIMEOUT 900)
endforeach()
