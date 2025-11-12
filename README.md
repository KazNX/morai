# Arachne fiber libraray

Arachne is a fibre or microthread library implemented in C++ using C++ coroutine support. Fibres
are a lightweight cooperative multi-tasking mechanism running in a single thread.

## The Arachne name

The name Arachne comes from Greek mythology. See [Wikipedia](https://en.wikipedia.org/wiki/Arachne).
The name was chosen because Arachne was a very skilled mortal weaver, which relates to fibres and
threads.

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
# Setting ARACHNE_BUILD_EXAMPLES is optional. Examples are off by default.
cmake -B build -G"Ninja Multi-Config" -DARACHNE_BUILD_EXAMPLES=ON
cmake --build build --config Release -j
# Or
cmake -B build -G"Ninja" -DCMAKE_BUILD_TYPE=Release -DARACHNE_BUILD_EXAMPLES=ON
cmake --build build -j
# Or
cmake -B build -DCMAKE_BUILD_TYPE=Release -DARACHNE_BUILD_EXAMPLES=ON
cmake --build build -j
```

Windows:

```powershell
# Start a Visual Studio shell (x64)
# Build with examples
# The toolchain and VCPKG manifest variables can be skipped if pdcurses is
# otherwise available on the system.
cmake -B build2 -G"Ninja Multi-Config" -DARACHNE_BUILD_EXAMPLES=ON `
  -DCMAKE_TOOLCHAIN_FILE="${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_MANIFEST_FEATURES=examples
cmake --build build --config Release -j
# Or build without examples
cmake -B build2 -G"Ninja Multi-Config"
cmake --build build --config Release -j
```

## TODO

- [ ] Usage examples
- [ ] Features to consider
  - [ ] Scheduling priority
    - [ ] Set on create
    - [ ] Adjustable
  - [ ] Reschedule yield statement - update again on a cycle
  - [ ] Deliberate update request by fibre ID(s)
