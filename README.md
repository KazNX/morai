# Arachne fiber libraray

Arachne is a fibre or microthread library implemented in C++ using C++ coroutine support. Fibres
are a lightweight cooperative multi-tasking mechanism running in a single thread.

## The Arachne name

The name Arachne comes from Greek mythology. See [Wikipedia](https://en.wikipedia.org/wiki/Arachne).
The name was chosen because Arachne was a very skilled mortal weaver, which relates to fibres and
threads.

## Requirements

- C++20 compatible compiler

## TODO

- [ ] Usage examples
- [ ] Features to consider
  - [ ] Scheduling priority
    - [ ] Set on create
    - [ ] Adjustable
  - [ ] Reschedule yield statement - update again on a cycle
  - [ ] Deliberate update request by fibre ID(s)
