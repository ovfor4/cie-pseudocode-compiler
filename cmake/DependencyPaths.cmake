# Recompute after extraction and after project() learns the library architecture.
macro(cpsc_dependency_paths build_directory)
    set(CPSC_LOCAL_PREFIXES "${build_directory}")
    foreach(prefix "${CPSC_DEPS_ROOT}" "${CPSC_DEPS_ROOT}/usr"
            "${CPSC_DEPS_ROOT}/mingw64" "${CPSC_DEPS_ROOT}/ucrt64"
            "${CPSC_DEPS_ROOT}/clang64")
        if(IS_DIRECTORY "${prefix}")
            list(APPEND CPSC_LOCAL_PREFIXES "${prefix}")
        endif()
    endforeach()
    file(GLOB llvm_prefixes LIST_DIRECTORIES TRUE "${CPSC_DEPS_ROOT}/usr/lib/llvm-*")
    file(GLOB brew_prefixes LIST_DIRECTORIES TRUE "${CPSC_DEPS_ROOT}/Cellar/*/*")
    list(APPEND CPSC_LOCAL_PREFIXES ${llvm_prefixes} ${brew_prefixes})

    set(CPSC_LOCAL_LIBRARY_DIRS)
    set(CPSC_LOCAL_INCLUDE_DIRS)
    set(CPSC_LOCAL_BIN_DIRS "${build_directory}")
    foreach(prefix IN LISTS CPSC_LOCAL_PREFIXES)
        list(APPEND CPSC_LOCAL_LIBRARY_DIRS "${prefix}/lib")
        list(APPEND CPSC_LOCAL_INCLUDE_DIRS "${prefix}/include" "${prefix}/include/libxml2")
        list(APPEND CPSC_LOCAL_BIN_DIRS "${prefix}/bin")
    endforeach()
    file(GLOB pandoc_directories LIST_DIRECTORIES TRUE "${CPSC_DEPS_ROOT}/pandoc-*")
    list(APPEND CPSC_LOCAL_BIN_DIRS ${pandoc_directories})
    if(CMAKE_LIBRARY_ARCHITECTURE)
        list(INSERT CPSC_LOCAL_LIBRARY_DIRS 0
            "${CPSC_DEPS_ROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
            "${CPSC_DEPS_ROOT}/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
        list(APPEND CPSC_LOCAL_INCLUDE_DIRS
            "${CPSC_DEPS_ROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}")
    endif()
endmacro()
