# Arachne fiber libraray

Arachne is a fibre or microthread library implemented in C++ using C++ coroutine support. Fibres
are a lightweight cooperative multi-tasking mechanism running in a single thread.

## The Arachne name

The name Arachne comes from Greek mythology. See [Wikipedia](https://en.wikipedia.org/wiki/Arachne).
The name was chosen because Arachne was a very skilled mortal weaver, which relates to fibres and
threads.

## Requirements

- C++23 modules compatible compiler
  - Only MSVC seems to work. Clang falls short mixing modules and gtest.
  - Clang experimental
    - Must set `-DCMAKE_CXX_FLAGS=-stdlib=libc++ -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld -stdlib=libc++`

## TODO

- [ ] Usage examples
