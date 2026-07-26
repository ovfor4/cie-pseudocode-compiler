cmake_minimum_required(VERSION 3.13)
project(cps_compiler)

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
)

add_library(CPSCodeGen
    lib/CodeGen/CodeGen.cc
    lib/CodeGen/ArrayHandler.cc
    lib/CodeGen/RuntimeCheck.cc
    lib/CodeGen/FunctionGen.cc
    lib/CodeGen/ArithmeticHandler.cc
    lib/CodeGen/StringHandler.cc
    lib/CodeGen/StringConversionHandler.cc
    lib/CodeGen/TypeSystem.cc
    lib/CodeGen/FileHandler.cc
)

llvm_map_components_to_libnames(llvm_libs core support native irreader)

add_executable(cpsc tools/driver/main.cc)
target_link_libraries(cpsc
    CPSCodeGen 
    CPSParser 
    CPSLexer 
    ${llvm_libs}
)
