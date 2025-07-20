[settings]
os=Linux
arch=armv8
compiler=gcc
compiler.version=13
compiler.libcxx=libstdc++11
compiler.cppstd=gnu17
build_type=Release

[buildenv]
# Cross-compiler tools
CC=/opt/homebrew/bin/aarch64-linux-gnu-gcc
CXX=/opt/homebrew/bin/aarch64-linux-gnu-g++
AR=/opt/homebrew/bin/aarch64-linux-gnu-ar
AS=/opt/homebrew/bin/aarch64-linux-gnu-as
LD=/opt/homebrew/bin/aarch64-linux-gnu-ld
STRIP=/opt/homebrew/bin/aarch64-linux-gnu-strip
RANLIB=/opt/homebrew/bin/aarch64-linux-gnu-ranlib

[options]
# Disable problematic features for cross-compilation
*:shared=False
*:fPIC=True
openssl/*:no-module=True
openssl/*:no-legacy=True
openssl/*:no-fips=True
openssl/*:no-async=True
openssl/*:no-tests=True
libcurl/*:with_ssl=openssl
libcurl/*:with_libssh2=False
libcurl/*:with_librtmp=False
libcurl/*:with_brotli=False
libcurl/*:with_zstd=False
libcurl/*:with_c_ares=False
libcurl/*:with_libpsl=False

[conf]
# CMake toolchain configuration
tools.cmake.cmaketoolchain:generator=Unix Makefiles
# Skip building tests for dependencies
tools.build:skip_test=True
# Cross-compilation CMake variables
tools.cmake.cmaketoolchain:system_name=Linux
tools.cmake.cmaketoolchain:system_processor=aarch64 