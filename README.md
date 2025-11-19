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
  - [x] Scheduling priority
    - [x] Set on create
    - [x] Allocated fixed priority queues in the scheduler
    - [x] Move fibres to nearest, lower bound priority
    - [x] Allow head insertion in fibre queues.
  - [ ] Reschedule yield statement - update again on a cycle
  - [ ] Multi-threaded
    - [ ] Priority queues, task based scheduler
      - Each thread picks up a fibre, runs one update cycle, back to job pool
      - Likely no head insertion
  - [ ] Cross thread scheduling
    - Multiple schedulers, one per thread
    - Fibre can elect to move between threads via `co_await move_to_scheduler(...);`
      - Could be on same thread, but works across threads
  - [ ] Await multiple fibres
  - [ ] Fibre "synchronisation"
    - [ ] Event objects like thread condition variables
    - [ ] Barrier
    - [ ] Latch
    - [ ] Semaphore
  - [ ] Logging interface

Priority scheduling requirements

- Ordered insertion of new fibres
- Removal and re-reinsertion of fibre during update loop
- Priority insertion of fibres during update loop
- Skip cancelled fibres during update loop

Options:

- Single fibre list, mutable during update iteration
- Multiple, mutable fibre lists (binned?)
- Immutable update list
  - Defer removal
  - Defer insertion
  - Integrate rescheduling
- Fibre queue based update
  - pop fibre
  - update
  - reinsert
  - stop when scheduling priority is lower than the last updated fibre
