# AGENTS.md - Rockchip RK3576 Android 14 Development Guide

This file contains essential information for agentic coding agents working in this Rockchip RK3576 Android 14 repository.

## Build Commands

### Environment Setup
```bash
# Source Android build environment
source build/envsetup.sh

# Choose target configuration
lunch <target>-<variant>
# Examples:
# lunch rk3576_r-userdebug
# lunch rk3576_r-user
# lunch rk3576_r-eng
```

### Primary Build Script (Rockchip Enhanced)
The main `build.sh` script provides comprehensive building with Rockchip-specific features:

```bash
# Complete build (U-Boot + Kernel + Android)
./build.sh -U -K -A -J$(nproc)

# Individual components
./build.sh -U              # Build U-Boot only
./build.sh -K              # Build Kernel only
./build.sh -C              # Build Kernel with Clang
./build.sh -A              # Build Android only
./build.sh -o              # Build OTA package only
./build.sh -u              # Build update.img only

# Build variants
./build.sh -A -v user      # Build user variant
./build.sh -A -v userdebug # Build userdebug variant (default)
./build.sh -A -v eng       # Build engineering variant

# Build with custom jobs
./build.sh -A -J16         # Build with 16 parallel jobs
```

### Traditional Android Build Commands
```bash
# Standard Android build
make -j$(nproc)                    # Parallel build
make installclean                  # Clean install directory
make dist                          # Build distribution files

# Build specific modules
make <module_name>                 # Build specific module
mmm <path_to_directory>            # Build module in directory
mm                                 # Build all modules in current directory

# Build with specific variants
make PRODUCT-<variant>             # Build specific product variant
```

### Kernel Building
```bash
# Set kernel version and architecture
export PRODUCT_KERNEL_VERSION=6.1
export PRODUCT_KERNEL_ARCH=arm64

# Build kernel with default config
cd kernel-6.1/
make ARCH=arm64 rockchip_defconfig
make ARCH=arm64 -j$(nproc)

# Build with Clang (recommended)
make ARCH=arm64 CC=clang LD=ld.lld LLVM=1 LLVM_IAS=1 -j$(nproc)
```

### U-Boot Building
```bash
cd u-boot/
make <board_defconfig>  # e.g., make rk3576_evb_defconfig
./make.sh
```

## Test Commands

### Atest (Android Test Executor)
```bash
# Run single test
atest <test_name>

# Run single test method
atest <test_class>#<test_method>

# Run with specific parameters
atest <test_name> -- --test-arg <key>:<value>

# Examples
atest libfs_avb_test              # Host tests
atest CtsGraphicsTestCases        # CTS tests
atest com.android.server.tests   # Framework tests
```

### CTS (Compatibility Test Suite)
```bash
# Build CTS
make cts

# Run CTS
./cts/tools/cts-tradefed/cts-tradefed

# In CTS console:
run cts                           # Run full suite
run cts --module <module>         # Run specific module
run cts --test <test_class>       # Run specific test class
```

### VTS (Vendor Test Suite)
```bash
# Build VTS
make vts

# Run VTS
./vts/tools/vts-tradefed/vts-tradefed

# In VTS console:
run vts                           # Run full suite
run vts --module <module>         # Run specific module
```

### Instrumentation Tests
```bash
# Run specific test class
adb shell am instrument -w -e class '<package>.<TestClass>' <test_package>/<runner>

# Run specific test method
adb shell am instrument -w -e class '<package>.<TestClass>#<testMethod>' <test_package>/<runner>
```

### Kernel Testing
```bash
# Kernel self-tests
cd kernel-6.1/tools/testing/selftests/
make run_tests

# KUnit tests
./kernel-6.1/tools/testing/kunit/kunit.py run
./kernel-6.1/tools/testing/kunit/kunit.py run --kunitconfig=lib/kunit
```

### U-Boot Testing
```bash
cd u-boot/
./test/py/test.py
```

## Code Style Guidelines

### C/C++ Code Style

#### Kernel Code (Linux Kernel Style)
- **Indentation**: 8 spaces, use tabs
- **Line Length**: 80 characters maximum
- **Pointers**: Right-aligned (`int *ptr`)
- **Braces**: Custom style - function braces on new line
- **Formatting**: Use `kernel-6.1/.clang-format`

```bash
# Format kernel code
cd kernel-6.1/
clang-format -i <file.c>
```

#### Android System Code (Google Style)
- **Indentation**: 4 spaces, no tabs
- **Line Length**: 100 characters maximum
- **Pointers**: Left-aligned (`int* ptr`)
- **Braces**: Same line for control statements
- **Formatting**: Use `system/core/.clang-format`

```bash
# Format Android system code
clang-format -i -style=file <file.cpp>
```

### Naming Conventions

#### Files and Directories
- **C/C++ files**: `lowercase_with_underscores.c/.cpp/.h`
- **Java files**: `PascalCase.java`
- **XML files**: `lowercase_with_underscores.xml`
- **Directories**: `lowercase_with_underscores`

#### Variables and Functions
- **C/C++**: `snake_case_function_name()`, `snake_case_variable`
- **Java**: `camelCaseMethod()`, `camelCaseVariable`, `PASCAL_CASE_CONSTANT`
- **Constants**: `UPPER_CASE_WITH_UNDERSCORES`

#### Classes and Structs
- **C++**: `PascalCase` for classes
- **C**: `snake_case` for structs, typedef `snake_case_t`

### Import Organization

#### C/C++ Includes
```cpp
// System headers first
#include <stdio.h>
#include <stdlib.h>

// Android system headers
#include <utils/Log.h>
#include <binder/IBinder.h>

// Local headers
#include "local_header.h"
#include "module/local_header.h"
```

#### Java Imports
```java
// Android framework imports first
import android.app.Activity;
import android.os.Bundle;

// Third-party libraries
import com.google.common.base.Preconditions;

// Project imports
import com.example.project.MyClass;
```

### Error Handling

#### C/C++
```cpp
// Use Android logging
#include <utils/Log.h>
#define LOG_TAG "MyModule"

// Return status codes
status_t result = some_function();
if (result != OK) {
    ALOGE("Function failed: %d", result);
    return result;
}

// Use DCHECK for debug builds
DCHECK(pointer != nullptr);
```

#### Java
```java
// Use proper exception handling
try {
    riskyOperation();
} catch (SpecificException e) {
    Log.e(TAG, "Operation failed", e);
    throw;
}

// Use parameter validation
Objects.requireNonNull(param, "param cannot be null");
```

### Documentation

#### C/C++ Doxygen Style
```cpp
/**
 * @brief Brief description of the function
 * @param param1 Description of parameter 1
 * @param param2 Description of parameter 2
 * @return Description of return value
 * @note Additional notes about usage
 */
status_t my_function(int param1, const char* param2);
```

#### Java Documentation
```java
/**
 * Brief description of the method.
 *
 * @param param1 description of parameter 1
 * @param param2 description of parameter 2
 * @return description of return value
 * @throws SpecificException when error occurs
 */
public ReturnType myMethod(int param1, String param2) {
    // implementation
}
```

## Android Build System (Soong/Blueprint)

### Android.bp Structure
```blueprint
cc_binary {
    name: "my_binary",
    srcs: ["src/*.cpp"],
    include_dirs: ["include"],
    shared_libs: ["libbase", "libutils"],
    cflags: ["-Wall", "-Werror"],
}
```

### Android.mk Structure
```makefile
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := my_module
LOCAL_SRC_FILES := src.cpp
LOCAL_SHARED_LIBRARIES := libbase libutils
include $(BUILD_SHARED_LIBRARY)
```

## Rockchip-Specific Features

### Device Configurations
- **TARGET_BOARD_PLATFORM**: Rockchip chipset (e.g., rk3576)
- **BOARD_ROCKCHIP_FLASH_TYPE**: Storage type (e.g., emmc, nvme)
- **BOARD_BUILD_GKI**: Enable Generic Kernel Image support
- **BOARD_USES_AB_IMAGE**: Enable A/B partition updates

### Build Variants
- **user**: Production build with minimal debugging
- **userdebug**: Production build with root access and debugging
- **eng**: Development build with full debugging capabilities

### Kernel Configuration
- **GKI Support**: Enabled for Android 14 compatibility
- **External WiFi Drivers**: Built as separate modules
- **Rockchip VPU Support**: Hardware video acceleration

## Development Workflow

### Code Changes
1. Make changes to source files
2. Run appropriate lint/format tools
3. Build affected modules: `mmm <path>`
4. Run relevant tests: `atest <test_name>`
5. Verify on device/emulator

### Common Build Issues
- Use `make installclean` between major changes
- Check `OUT_DIR` environment variable for build output
- Ensure proper Java version: OpenJDK 8 for Android 14
- Use `lunch` to set target before building

### Rockchip-Specific Debugging
- Enable serial console in U-Boot for early debugging
- Use `dmesg` to check kernel messages
- Rockchip tools in `RKTools/` for firmware manipulation
- Check `rockdev/Image-<target>/` for build artifacts