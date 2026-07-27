# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is **mh_stuff**, a C++20 utility library containing reusable components for system programming. The library operates in either header-only mode or as a compiled static/shared library, with minimal interdependencies between modules to support selective usage.

**Important**: This library experiences significant API churn with no stability guarantees. It's designed for the author's personal projects and regular structural changes should be expected.

## Build and Test Commands

This project uses CMake with CPM (CMake Package Manager) for dependency management and Just for build task automation. The main dependencies are `mh-cmake-common` for build utilities and `Catch2` for testing.

### Building the Project

```bash
# Using justfile (recommended)
just build

# Or using CMake presets directly
cmake --preset default
cmake --build --preset default

# Or standard CMake build
mkdir build && cd build
cmake .. -G Ninja
cmake --build .
```

### Running Tests

The project uses Catch2 for testing:

```bash
# Using justfile (recommended)
just test

# Or using CMake presets
ctest --preset default

# Or from build directory
cd build && ctest --output-on-failure

# Or run the test executable directly
./build/mh_stuff_tests
```

### Build Configuration Options

- `MH_STUFF_BUILD_SHARED_LIBS=ON/OFF` - Build as shared library instead of static
- `MH_STUFF_COMPILE_LIBRARY=ON/OFF` - Compile implementations vs header-only mode
- `BUILD_TESTING=ON/OFF` - Enable/disable test compilation

## Architecture Overview

### Module Organization

The library is organized into logical modules under `cpp/include/mh/`:

- **`coroutine/`** - C++20 coroutine infrastructure including `task<T>` for async operations
- **`error/`** - Error handling with `mh_ensure()` assertions and `expected<T,E>` monadic error types
- **`concurrency/`** - Thread pools, async operations, and synchronization primitives
- **`io/`** - Platform-agnostic async I/O with `source`/`sink` abstractions
- **`memory/`** - Smart pointers, buffers, and memory management utilities
- **`text/`** - String processing, formatting (fmtlib/std::format), and text utilities
- **`data/`** - Data structures like `lazy<T>`, optional references, and bit manipulation
- **`process/`** - Process management and async process I/O
- **`math/`** - Mathematical utilities including interpolation and random number generation
- **`reflection/`** - Compile-time reflection for enums and structs

### Key Design Patterns

1. **Header-Only with Selective Compilation**: `.hpp` files contain interfaces, `.inl` files contain implementations that can be compiled into a library or included directly
2. **C++20 Coroutine-Based Async**: The `task<T>` type provides future-like semantics with coroutine support for async operations
3. **Platform Abstraction**: Cross-platform code with platform-specific implementations hidden behind clean interfaces
4. **Compile-Time Feature Detection**: Extensive use of `__has_include` and feature test macros for conditional compilation
5. **RAII and Exception Safety**: Strong exception safety guarantees throughout with automatic resource management

### Build System Notes

- Uses CPM (CMake Package Manager) for automatic dependency fetching (Catch2 testing framework)
- Automatically generates a library.cpp file from all .inl files when in compiled library mode  
- Tests include compile-time verification that all headers can be included independently
- Supports coverage reporting with gcov/gcovr on GCC/Clang

### Important Compile-Time Configurations

- `MH_COMPILE_LIBRARY` - Controls whether implementations are compiled into a library or included header-only
- `MH_COROUTINES_SUPPORTED` - Enables C++20 coroutine functionality 
- `MH_FORMATTER` - Selects formatting backend (fmtlib, std::format, or none)
- `MH_BROKEN_UNICODE=1` - Forced workaround for compiler Unicode issues

## Development Guidelines

- This library requires C++20 and uses modern C++ features extensively
- Platform support includes Windows (MSVC), Linux (GCC), and macOS (Clang)
- Code follows RAII principles with move semantics preferred over copying
- Exception safety is critical - all code should provide strong exception safety guarantees
- The library is designed for zero-overhead abstractions with extensive use of `constexpr`

## Testing

Tests are located in the `test/` directory and use Catch2. The build system:
- Automatically discovers all `*_test.cpp` files
- Creates a unified test executable `mh_stuff_tests`
- Includes compile-time tests to verify all headers can be included independently
- Conditionally excludes platform-specific tests (e.g., `io_getopt_test.cpp` on systems without getopt)