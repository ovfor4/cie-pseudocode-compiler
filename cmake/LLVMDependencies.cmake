cpsc_dependency_paths("${CMAKE_BINARY_DIR}")
list(INSERT CMAKE_PREFIX_PATH 0 ${CPSC_LOCAL_PREFIXES})
list(INSERT CMAKE_LIBRARY_PATH 0 ${CPSC_LOCAL_LIBRARY_DIRS})
list(INSERT CMAKE_INCLUDE_PATH 0 ${CPSC_LOCAL_INCLUDE_DIRS})

cpsc_find_dependency(PATH LLVM_DIR NAMES LLVMConfig.cmake
    PATH_SUFFIXES lib/cmake/llvm cmake)
# find_package(LLVM REQUIRED CONFIG) also checks standard system package layouts.
cpsc_find_dependency(PATH zstd_DIR NAMES zstdConfig.cmake
    PATH_SUFFIXES lib/cmake/zstd "lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/zstd")
cpsc_find_library(ZLIB_LIBRARY_RELEASE z)
cpsc_find_library(zstd_LIBRARY zstd zstd_static)
cpsc_find_library(zstd_STATIC_LIBRARY libzstd.a libzstd_static.a)
cpsc_find_library(Terminfo_LIBRARIES terminfo tinfo curses ncurses ncursesw)
cpsc_find_library(FFI_LIBRARIES ffi)
cpsc_find_library(FFI_STATIC_LIBRARIES libffi.a)
cpsc_find_library(LibEdit_LIBRARIES edit)
cpsc_find_library(LIBXML2_LIBRARY xml2)
cpsc_find_include(ZLIB_INCLUDE_DIR zlib.h)
cpsc_find_include(zstd_INCLUDE_DIR zstd.h)
cpsc_find_include(FFI_INCLUDE_DIRS ffi.h)
cpsc_find_include(LibEdit_INCLUDE_DIRS histedit.h)
cpsc_find_include(LIBXML2_INCLUDE_DIR libxml/xpath.h)
