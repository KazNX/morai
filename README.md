# Morai fibre library

![Build](https://github.com/KazNX/morai/actions/workflows/ci.yml/badge.svg)

Morai is a C++ fibre or microthread library using C++ coroutines. Fibres are a lightweight
cooperative multi-tasking mechanism running in a single thread.

## The Morai name

The name Morai comes from Greek mythology. See [Wikipedia](https://en.wikipedia.org/wiki/Moirai).
Morai is the name of the Three Fates, the weavers of destiny. The name is chose as weaving relates
to fibres and threads.

## Requirements

- C++20 compatible compiler with coroutine support
- CMake 3.16+
- Ninja build (recommended, assumed installed)
- Linux/MacOS
  - Optional libraries to build examples:
    - libncurses
- Windows:
  - Optional: vcpkg to build examples

To install optional requirements on Ubuntu:

```bash
sudo apt install -y libncurses-dev
```

## Build

Linux/MacOS:

```bash
# Setting MORAI_BUILD_EXAMPLES is optional. Examples are off by default.
cmake -B build -G"Ninja Multi-Config" -DMORAI_BUILD_EXAMPLES=ON
cmake --build build --config Release -j
# Or
cmake -B build -G"Ninja" -DCMAKE_BUILD_TYPE=Release -DMORAI_BUILD_EXAMPLES=ON
cmake --build build -j
# Or
cmake -B build -DCMAKE_BUILD_TYPE=Release -DMORAI_BUILD_EXAMPLES=ON
cmake --build build -j
```

Windows:

```powershell
# Start a Visual Studio shell (x64)
# Build with examples
# The toolchain and VCPKG manifest variables can be skipped if pdcurses is
# otherwise available on the system.
cmake -B build2 -G"Ninja Multi-Config" -DMORAI_BUILD_EXAMPLES=ON `
  -DCMAKE_TOOLCHAIN_FILE="${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_MANIFEST_FEATURES=examples
cmake --build build --config Release -j
# Or build without examples
cmake -B build2 -G"Ninja Multi-Config"
cmake --build build --config Release -j
```

## Overview

See [Morai overview](./docs/morai.md).
