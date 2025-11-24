# Morai

Morai is a fibre or microthread library implemented in C++ using C++ coroutine support. Fibres
are a lightweight cooperative multi-tasking mechanism running in a single thread. Morai supports the
following key features:

- Single thread scheduling via `morai::Scheduler`.
- Job pool scheduling using `morai::ThreadPool`.
  - Option to call updates from external threads.
- Moving fibres between schedulers.

## Writing fibres

A Morai `morai::Fibre` entry point is any function that:

- Has a `morai::Fibre` return type
- Uses `co_yield`, `co_await` and/or `co_return`.

Bellow is an example `Fibre` that prints "Hello" and "world", one word each time it is resumed, then
exists. The `main()` function creates a scheduler for this fibre and runs it.

```c++
#include <morai/Fibre.hpp>
#include <morai/Scheduler.hpp>

morai::Fibre hello_world()
{
  std::cout << "Hello\n";
  co_yield {};
  std::cout << "world\n";
  // Implicit co_return;
}

int main()
{
  morai::Scheduler scheduler;

  scheduler.start(hello_world()); // Note we call the hello_world function

  while (!scheduler.empty())
  {
    scheduler.update(0.0); // "time" is frozen for this example.
  }
}

// Output:
/*
Hello
world
*/
```

A Fibre can be thought of a kind of state machine where the current state is inherently embedded in
the local variables, the program counter and any externally visible variables - that is any
parameters passed to the function or variables captured by a lambda based fibre coroutine.

## Yielding and waiting

## Things to do with fibres

- Use `gsl::finally()` or `morai::finally()` for guaranteed cleanup code.

## Pitfalls

- Lifetime and ownership - passing or capturing references
