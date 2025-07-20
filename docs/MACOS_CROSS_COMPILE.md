# Cross-Compiling from macOS to Linux ARM64

This guide explains how to cross-compile the Greengrass Provisioning Service from macOS to Linux ARM64 using Conan 2.0 and the GNU ARM toolchain.

## Prerequisites

### 1. Install the ARM Cross-Compiler Toolchain

On macOS, install the ARM64 cross-compiler using Homebrew:

```bash
brew install aarch64-linux-gnu-gcc
```

This will install the complete GNU toolchain for ARM64 Linux, including:
- `aarch64-linux-gnu-gcc` (C compiler)
- `aarch64-linux-gnu-g++` (C++ compiler)  
- `aarch64-linux-gnu-ar` (Archiver)
- `aarch64-linux-gnu-ld` (Linker)
- And other binary utilities

The tools are typically installed in `/opt/homebrew/bin/` on Apple Silicon Macs.

### 2. Verify Installation

```bash
which aarch64-linux-gnu-gcc
# Expected output: /opt/homebrew/bin/aarch64-linux-gnu-gcc

aarch64-linux-gnu-gcc --version
# Expected output: aarch64-linux-gnu-gcc (GCC) 13.3.0 or similar
```

## Setting up Conan Profile for Cross-Compilation

### 1. Create a Cross-Compilation Profile

Create a file named `ubuntu-arm-cross.profile` with the following content:

```ini
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
```

### 2. Key Profile Sections Explained

#### [settings]
- `os=Linux`: Target operating system
- `arch=armv8`: Target architecture (ARM 64-bit)
- `compiler.version=13`: Should match your cross-compiler version

#### [buildenv]
Sets environment variables that CMake and other build tools will use:
- `CC`, `CXX`: C and C++ compilers
- `AR`, `RANLIB`, `STRIP`: Binary utilities for creating archives and stripping symbols
- These paths should match where Homebrew installed your toolchain

#### [conf]
- `tools.cmake.cmaketoolchain:system_name=Linux`: Tells CMake we're cross-compiling for Linux
- `tools.cmake.cmaketoolchain:system_processor=aarch64`: Sets the target processor architecture

## Building the Project

### 1. Clean Build Directory
```bash
rm -rf build
mkdir build
cd build
```

### 2. Install Dependencies with Cross-Compilation Profile
```bash
conan install .. --profile:build=default --profile:host=../ubuntu-arm-cross.profile --build=missing
```

This command:
- Uses your default profile for the **build** context (tools that run on macOS)
- Uses the cross-compilation profile for the **host** context (libraries for Linux ARM64)
- `--build=missing`: Builds any dependencies that aren't available as pre-built binaries

### 3. Activate Build Environment
```bash
source Release/generators/conanbuild.sh
```

This sets up the environment variables defined in the `[buildenv]` section of your profile.

### 4. Configure with CMake
```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
```

The Conan-generated toolchain file contains all the cross-compilation settings.

### 5. Build
```bash
make -j$(nproc)
```

### 6. Verify the Binary
```bash
file greengrass_provisioning_service
```

Expected output:
```
greengrass_provisioning_service: ELF 64-bit LSB executable, ARM aarch64, version 1 (GNU/Linux), 
dynamically linked, interpreter /lib/ld-linux-aarch64.so.1, ...
```

## Deployment Considerations

### Library Compatibility

When cross-compiling with a newer GCC version (e.g., GCC 13) for systems with older GCC versions, you might encounter GLIBCXX version mismatches. For example:

```
./greengrass_provisioning_service: /lib/aarch64-linux-gnu/libstdc++.so.6: version `GLIBCXX_3.4.32' not found
```

**Solutions:**

1. **Use a Docker container with matching GCC version**: Update your runtime container to a newer base image:
   ```dockerfile
   FROM ubuntu:24.04  # Has newer libstdc++ that includes GLIBCXX_3.4.32
   ```

2. **Static linking** (if binary size is not a concern): Add to your CMakeLists.txt:
   ```cmake
   if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
       target_link_options(${PROJECT_NAME} PRIVATE -static-libgcc -static-libstdc++)
   endif()
   ```

3. **Use an older cross-compiler**: Install a cross-compiler that matches your target system's GCC version.

### Running on Target

1. Copy the binary to your Linux ARM64 device:
   ```bash
   scp build/greengrass_provisioning_service user@target-device:/path/to/destination
   ```

2. Ensure required shared libraries are available on the target system.

3. Make the binary executable and run:
   ```bash
   chmod +x greengrass_provisioning_service
   ./greengrass_provisioning_service
   ```

## Troubleshooting

### Missing Cross-Compiler
If CMake can't find the cross-compiler, ensure:
1. The paths in your profile's `[buildenv]` section are correct
2. You've sourced the `conanbuild.sh` script before running CMake

### Dependency Build Failures
Some dependencies might fail to cross-compile. The profile includes options to disable problematic features:
- OpenSSL modules and tests are disabled
- libcurl optional features are disabled
- All libraries are built as static by default (`*:shared=False`)

### Architecture Mismatch
Ensure your profile's `arch` setting matches your target:
- `armv8`: 64-bit ARM (ARM64/AArch64)
- `armv7` or `armv7hf`: 32-bit ARM

## Benefits of This Approach

1. **Self-contained profiles**: All cross-compilation settings are in the Conan profile
2. **Reproducible builds**: Profile can be version-controlled and shared
3. **Conan handles complexity**: Dependency management and toolchain generation are automated
4. **Standard Conan 2.0 workflow**: Following best practices for cross-compilation 