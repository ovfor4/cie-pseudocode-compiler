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
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(CPSLexer STATIC lib/Lexer/Lexer.cc)
add_library(CPSParser STATIC
    lib/Parser/Parser.cc
    lib/Parser/ParserFunction.cc
    lib/Parser/ParserFile.cc
    lib/Parser/ParserType.cc
)

add_library(CPSCodeGen STATIC
    lib/CodeGen/CodeGen.cc
    lib/CodeGen/Designator.cc
    lib/CodeGen/CTypeGen.cc
    lib/CodeGen/ArrayHandler.cc
    lib/CodeGen/RuntimeCheck.cc
    lib/CodeGen/ExceptionHandler.cc
    lib/CodeGen/FunctionGen.cc
    lib/CodeGen/ArithmeticHandler.cc
    lib/CodeGen/StringHandler.cc
    lib/CodeGen/StringConversionHandler.cc
    lib/CodeGen/TypeSystem.cc
    lib/CodeGen/FileHandler.cc
)

llvm_map_components_to_libnames(llvm_libs core support)
if(CPSC_RUN_MODE STREQUAL "EMBEDDED")
    llvm_map_components_to_libnames(llvm_libs core support native mcjit)
    foreach(llvm_lib IN LISTS llvm_libs)
        get_target_property(llvm_type ${llvm_lib} TYPE)
        if(NOT llvm_type STREQUAL "STATIC_LIBRARY")
            message(FATAL_ERROR "EMBEDDED requires static LLVM libraries: ${llvm_lib}")
        endif()
    endforeach()
    include(cmake/EmbeddedLLVM.cmake)
endif()

add_executable(cpsc tools/driver/main.cc)
target_compile_definitions(cpsc PRIVATE CPSC_RUN_${CPSC_RUN_MODE})
if(CPSC_RUN_MODE STREQUAL "EMBEDDED")
    target_sources(cpsc PRIVATE tools/driver/EmbeddedRunner.cc)
    if(MINGW)
        target_link_options(cpsc PRIVATE -static)
    elseif(UNIX AND NOT APPLE)
        target_link_options(cpsc PRIVATE -static-libstdc++ -static-libgcc -Wl,--as-needed)
    endif()
endif()
target_link_libraries(cpsc
    CPSCodeGen 
    CPSParser 
    CPSLexer 
    ${llvm_libs}
)

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
