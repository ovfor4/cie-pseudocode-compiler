cmake_minimum_required(VERSION 3.13)
project(cps_compiler)

set(CPSC_RUN_MODE "" CACHE STRING "Execution mode: PATH or NONE (required)")
set_property(CACHE CPSC_RUN_MODE PROPERTY STRINGS PATH NONE)
if(NOT CPSC_RUN_MODE MATCHES "^(PATH|NONE)$")
    message(FATAL_ERROR "Specify -DCPSC_RUN_MODE=PATH or -DCPSC_RUN_MODE=NONE")
endif()

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(CPSLexer lib/Lexer/Lexer.cc)
add_library(CPSParser
    lib/Parser/Parser.cc
    lib/Parser/ParserFunction.cc
    lib/Parser/ParserFile.cc
    lib/Parser/ParserType.cc
)

add_library(CPSCodeGen
    lib/CodeGen/CodeGen.cc
    lib/CodeGen/Designator.cc
    lib/CodeGen/ArrayHandler.cc
    lib/CodeGen/RuntimeCheck.cc
    lib/CodeGen/FunctionGen.cc
    lib/CodeGen/ArithmeticHandler.cc
    lib/CodeGen/StringHandler.cc
    lib/CodeGen/StringConversionHandler.cc
    lib/CodeGen/TypeSystem.cc
    lib/CodeGen/FileHandler.cc
)

llvm_map_components_to_libnames(llvm_libs core support)

add_executable(cpsc tools/driver/main.cc)
target_compile_definitions(cpsc PRIVATE CPSC_RUN_${CPSC_RUN_MODE})
target_link_libraries(cpsc
    CPSCodeGen 
    CPSParser 
    CPSLexer 
    ${llvm_libs}
)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/doc/cpsc.1"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/doc"
    COMMAND pandoc --standalone --from=markdown --to=man
        "${CMAKE_CURRENT_SOURCE_DIR}/doc/cpsc.md"
        --output "${CMAKE_CURRENT_BINARY_DIR}/doc/cpsc.1"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/doc/cpsc.md"
    COMMENT "Generating cpsc(1) manual"
    VERBATIM
)
add_custom_target(doc DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/doc/cpsc.1")
