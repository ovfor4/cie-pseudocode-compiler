function(cpsc_prepare_windows_packages)
    # Keep the MSYS2 prefix layout so relative CMake imports and DLL paths work.
    file(GLOB_RECURSE configs "${CPSC_DEPS_ROOT}/*.cmake")
    foreach(config IN LISTS configs)
        file(READ "${config}" contents)
        foreach(prefix mingw64 ucrt64 clang64)
            string(REPLACE "C:/msys64/${prefix}/" "${CPSC_DEPS_ROOT}/${prefix}/"
                contents "${contents}")
            string(REPLACE "\"/${prefix}/" "\"${CPSC_DEPS_ROOT}/${prefix}/"
                contents "${contents}")
        endforeach()
        file(WRITE "${config}" "${contents}")
    endforeach()
endfunction()
