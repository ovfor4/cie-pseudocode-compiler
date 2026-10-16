#!/bin/sh
# Build LLD against an installed LLVM SDK; install it in cpsc's build prefix.
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "Usage: sh tools/build-static-lld.sh LLVM_PREFIX BUILD_DIRECTORY [JOBS]" >&2
    exit 1
fi

llvm_prefix=$(cd "$1" && pwd)
mkdir -p "$2"
build_root=$(cd "$2" && pwd)
jobs=${3:-3}
llvm_version=$("$llvm_prefix/bin/llvm-config" --version)
case "$llvm_version" in
    23.*) ;;
    *) echo "Static LLD requires LLVM 23; found $llvm_version" >&2; exit 1 ;;
esac

work="$build_root/static-lld-$llvm_version"
archive="$work/llvm-project-$llvm_version.src.tar.xz"
mkdir -p "$work/src/llvm"
if [ ! -f "$archive" ]; then
    curl -fL --retry 3 \
        "https://github.com/llvm/llvm-project/releases/download/llvmorg-$llvm_version/llvm-project-$llvm_version.src.tar.xz" \
        -o "$archive.part"
    mv "$archive.part" "$archive"
fi
if [ ! -f "$work/src/.extracted" ]; then
    tar -xJf "$archive" -C "$work/src" --strip-components=1 \
        "llvm-project-$llvm_version.src/lld" "llvm-project-$llvm_version.src/cmake" \
        "llvm-project-$llvm_version.src/libunwind/include"
    touch "$work/src/.extracted"
fi

set --
if [ "$(uname -s)" = Darwin ]; then
    set -- "-DCMAKE_OSX_ARCHITECTURES=$(uname -m)" \
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-15.0}"
fi
cmake -S "$work/src/lld" -B "$work/build" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DLLVM_INCLUDE_TESTS=OFF -DLLVM_LINK_LLVM_DYLIB=ON \
    "-DLLVM_DIR=$llvm_prefix/lib/cmake/llvm" \
    "-DLLVM_MAIN_SRC_DIR=$work/src/llvm" \
    "-DCMAKE_C_COMPILER=$llvm_prefix/bin/clang" \
    "-DCMAKE_CXX_COMPILER=$llvm_prefix/bin/clang++" \
    "-DCMAKE_INSTALL_RPATH=$llvm_prefix/lib" \
    "-DCMAKE_INSTALL_PREFIX=$build_root" -DCMAKE_INSTALL_LIBDIR=lib "$@"
cmake --build "$work/build" --parallel "$jobs"
cmake --install "$work/build"
