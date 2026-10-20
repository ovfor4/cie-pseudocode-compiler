cmake_minimum_required(VERSION 3.13)
include(cmake/LocalDependencies.cmake)
include(cmake/BuildTools.cmake)
project(cps_compiler)

foreach(language C CXX)
    if(NOT CMAKE_${language}_COMPILER_ID STREQUAL "Clang"
            OR CMAKE_${language}_COMPILER_VERSION VERSION_LESS 23
            OR NOT CMAKE_${language}_COMPILER_VERSION VERSION_LESS 24)
        message(FATAL_ERROR
            "cpsc requires Clang 23 for ${language}; found ${CMAKE_${language}_COMPILER_ID} ${CMAKE_${language}_COMPILER_VERSION} (${CMAKE_${language}_COMPILER})")
    endif()
endforeach()

if(WIN32 AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    if("$ENV{LOCALAPPDATA}" STREQUAL "")
        message(FATAL_ERROR "LOCALAPPDATA is unset; specify -DCMAKE_INSTALL_PREFIX explicitly")
    endif()
    file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}/Programs/cpsc" cpsc_default_prefix)
    set(CMAKE_INSTALL_PREFIX "${cpsc_default_prefix}" CACHE PATH "Installation directory" FORCE)
endif()

set(CPSC_RUN_MODE "" CACHE STRING "Execution mode: PATH, EMBEDDED or NONE (required)")
set_property(CACHE CPSC_RUN_MODE PROPERTY STRINGS PATH EMBEDDED NONE)
if(NOT CPSC_RUN_MODE MATCHES "^(PATH|EMBEDDED|NONE)$")
    message(FATAL_ERROR "Specify -DCPSC_RUN_MODE=PATH, EMBEDDED or NONE")
endif()

include(cmake/LLVMDependencies.cmake)
find_package(LLVM REQUIRED CONFIG)
if(NOT LLVM_VERSION_MAJOR EQUAL 23)
    message(FATAL_ERROR "cpsc requires LLVM 23; found LLVM ${LLVM_PACKAGE_VERSION} (${LLVM_DIR})")
endif()
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
add_definitions(${LLVM_DEFINITIONS_LIST})

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(CPSLexer STATIC lib/Lexer/Lexer.cc)
add_library(CPSParser STATIC
    lib/Parser/Parser.cc
    lib/Parser/ParserExpression.cc
    lib/Parser/ParserControlFlow.cc
    lib/Parser/ParserFunction.cc
    lib/Parser/ParserFile.cc
    lib/Parser/ParserType.cc
)

add_library(CPSCodeGen STATIC
    lib/CodeGen/CodeGen.cc
    lib/CodeGen/ExpressionGen.cc
    lib/CodeGen/OperatorGen.cc
    lib/CodeGen/StatementGen.cc
    lib/CodeGen/GlobalGen.cc
    lib/CodeGen/ConstantGen.cc
    lib/CodeGen/ControlFlowGen.cc
    lib/CodeGen/ConstructorGen.cc
    lib/CodeGen/ValueTypeGen.cc
    lib/CodeGen/OutputGen.cc
    lib/CodeGen/Designator.cc
    lib/CodeGen/TypeCheckGen.cc
    lib/CodeGen/CTypeGen.cc
    lib/CodeGen/ArrayHandler.cc
    lib/CodeGen/ArrayParameters.cc
    lib/CodeGen/ArrayOutput.cc
    lib/CodeGen/RuntimeCheck.cc
    lib/CodeGen/ExceptionHandler.cc
    lib/CodeGen/FunctionGen.cc
    lib/CodeGen/ArithmeticHandler.cc
    lib/CodeGen/StringHandler.cc
    lib/CodeGen/StringConversionHandler.cc
    lib/CodeGen/TypeSystem.cc
    lib/CodeGen/NativeTypeSystem.cc
    lib/CodeGen/MemoryHandler.cc
    lib/CodeGen/FileHandler.cc
    lib/CodeGen/FileRuntimeIO.cc
)

llvm_map_components_to_libnames(llvm_libs core support native passes object)
if(CPSC_RUN_MODE STREQUAL "EMBEDDED")
    llvm_map_components_to_libnames(llvm_libs core support native passes object orcjit jitlink orctargetprocess executionengine)
endif()
foreach(llvm_lib IN LISTS llvm_libs)
    get_target_property(llvm_type ${llvm_lib} TYPE)
    if(NOT llvm_type STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR "cpsc requires static LLVM libraries: ${llvm_lib}")
    endif()
endforeach()
include(cmake/EmbeddedLLVM.cmake)

add_executable(cpsc tools/driver/main.cc)
target_compile_definitions(cpsc PRIVATE CPSC_RUN_${CPSC_RUN_MODE})
if(CPSC_RUN_MODE STREQUAL "EMBEDDED")
    target_sources(cpsc PRIVATE
        tools/driver/EmbeddedRunner.cc
        tools/driver/MinGWJIT.cc)
endif()
if(MINGW)
    target_link_options(cpsc PRIVATE -static)
elseif(APPLE)
    # LLVM's support interface can include unused Homebrew dylibs such as Z3.
    target_link_options(cpsc PRIVATE -Wl,-dead_strip_dylibs)
elseif(UNIX AND NOT APPLE)
    target_link_options(cpsc PRIVATE -static-libstdc++ -static-libgcc -Wl,--as-needed)
endif()
target_link_libraries(cpsc PRIVATE
    CPSCodeGen 
    CPSParser 
    CPSLexer 
    ${llvm_libs}
)

include(cmake/NativeCompiler.cmake)

# Build the experimental pseudocode library with the same host C++ ABI.
file(GLOB cpsc_library_cc CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/pseudocode-lib/*.cc")
file(GLOB cpsc_library_sources CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/pseudocode-lib/*.cps")
set(cpsc_library_outputs)
foreach(source IN LISTS cpsc_library_cc)
    get_filename_component(name "${source}" NAME_WE)
    set(output "${CMAKE_BINARY_DIR}/pseudocode-lib/${name}.o")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/pseudocode-lib"
        COMMAND "${CMAKE_CXX_COMPILER}" -c -O2 -fPIC -std=c++17 "${source}" -o "${output}"
        DEPENDS "${source}"
        VERBATIM)
    list(APPEND cpsc_library_outputs "${output}")
endforeach()
foreach(source IN LISTS cpsc_library_sources)
    get_filename_component(name "${source}" NAME)
    set(output "${CMAKE_BINARY_DIR}/pseudocode-lib/${name}")
    add_custom_command(OUTPUT "${output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/pseudocode-lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${source}" "${output}"
        DEPENDS "${source}"
        VERBATIM)
    list(APPEND cpsc_library_outputs "${output}")
endforeach()
add_custom_target(pseudocode-lib ALL DEPENDS ${cpsc_library_outputs})

set(cpsc_embedded_library "${CMAKE_BINARY_DIR}/standard-library/StandardLibrary.inc")
add_custom_command(OUTPUT "${cpsc_embedded_library}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/standard-library"
    COMMAND "${PERL_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/driver/embed-standard-library.pl"
        "${cpsc_embedded_library}" ${cpsc_library_outputs}
    DEPENDS ${cpsc_library_outputs} tools/driver/embed-standard-library.pl
    VERBATIM)
target_sources(cpsc PRIVATE tools/driver/StandardLibrary.cc "${cpsc_embedded_library}")
target_include_directories(cpsc PRIVATE "${CMAKE_BINARY_DIR}/standard-library")

option(BUILD_TESTING "Enable compiler conformance tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

include(cmake/Manual.cmake)

configure_file(cmake/Install.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/Install.cmake" @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/Install.cmake")

set(cpsc_release_system "${CMAKE_SYSTEM_NAME}")
if(WIN32)
    set(cpsc_release_system Windows)
endif()
add_custom_target(release
    COMMAND "${CMAKE_COMMAND}"
        "-DCPSC_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DCPSC_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
        "-DCPSC_EXECUTABLE=$<TARGET_FILE:cpsc>"
        "-DCPSC_PLATFORM=${cpsc_release_system}-${CMAKE_SYSTEM_PROCESSOR}"
        "-DCPSC_WINDOWS=${WIN32}"
        "-DCPSC_LOCAL_BIN_DIRS=${CPSC_LOCAL_BIN_DIRS}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Release.cmake"
    DEPENDS cpsc doc
    COMMENT "Packaging cpsc and its manual"
    VERBATIM
)
