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
parameters passed to the function or variables captured by a lambda based fibre coroutine. Fibres
offer a simpler way of writing logic that depends on other things occurring at some point in time,
where it is now well known when or if that other thing will occur.

For example, the game code below belongs to simple turret object, written without fibres.

```c++
void Turret::update(float dt)
{
  if (dead())
  {
    return;
  }

  if (auto target = getBestTarget())
  {
    turnTo(target, dt);
    if (aligned(target))
    {
      fireAt(target, dt);
    }
  }
}
```

This code assumes the `Turret::update()` function is called regularly. ...

Below is a fibre based version. One thing to note is how easily we can separate various parts of the
logic into isolated responsibilities. Additionally we have reduced nesting and each the code for
each fibre flows more naturally.

```c++
morai::Fibre Turret::deadWatchFibre()
{
  co_await [this]() { return isDead(); };
  cancelFibres();
}

morai::Fibre Turret::trackingFibre()
{
  for (;;)
  {
    auto target = getBestTarget();
    turnTo(target, getDt());
    co_yield {};
  }
}

morai::Fibre Turret::aimAndFireFibre()
{
  for (;;)
  {
    co_await [this] () { return aligned(getBestTarget()); }
    fireAt(target, dt);
    co_yield {};
  }
}
```

### Fibre cleanup

Fibres are unwound like any other C++ scope. This means that when a fibre's local variables are all
destroyed when the thread is destroyed. This makes fibres
[RAII](https://en.cppreference.com/w/cpp/language/raii.html) safe.

This RAII safety can be used to effect a pseudo destructor in a `Fibre` using `gsl::finally()` or
`morai::fibre()` (the Morai version is a copy of the [GSL](https://github.com/microsoft/GSL)
version). The example below shows how to use how `finally()` callbacks are always invoked.

```c++
morai::Fibre pseudoDestructor()
{
  const auto pseudo_destruct = morai::finally([]() {
    std::cout << "Goodbye fibre\n";
  });

  co_yield std::chrono::seconds(5);
}

// After 5s of running, the fibre is cleaned up and we will see the messsage:
// Goodbye fibre
```

## Schedulers

There are two schedulers in the core Morai library - `Scheduler` and `ThreadPool`. The `Scheduler`
is a single threaded scheduler, while the `TheadPool` is a threadsafe, worker based scheduler. The
table below shows some different features of the two schedulers.

| Feature             | `Scheduler`           | `ThreadPool`                   |
| ------------------- | --------------------- | ------------------------------ |
| Threading           | Single threaded       | Multiple workers               |
| Priority scheduling | fixed priority values | fixed priority values          |
| Queue sizing        | Growable              | Fixed size                     |
| Start fibres        | `start()`             | `start()`                      |
| Cancel fibre by Id  | yes                   | no                             |
| Cancel all          | yes                   | yes                            |
| Move to             | yes - threadsafe      | yes - threadsafe               |
| Threadsafe update   | no                    | yes                            |
| Explicit update     | only                  | optional, time sliced          |
| External threads    | N/A                   | optional - explicit `update()` |

## Epoch time

The default `Scheduler` class is a single threaded fibre scheduler that attempts to resume all
fibres during a `Scheduler::update()` call. This function accepts a `double` value which is treated
as an epoch time - it is expected to be monotonic increasing. The simplest way to update the
`Scheduler` is to use a `std::chrono` clock as shown below.

```c++
#include <morai/Scheduler.hpp>

#include <chrono>
#include <thread>

void schedulerLoop1(morai::Scheduler& scheduler)
{
  for (;;)
  {
    const auto now = std::chrono::steady_clock::now();  // Or std::chrono::system_clock
    const double epoch_s = std::chrono::duration_cast<std::chrono::duration<double>>(now);
    scheduler.update(epoch_s);
    std::this_thread::sleep_until(now + std::chrono::milliseconds(1000 / 60));
  }
}

void schedulerLoop2(morai::Scheduler& scheduler)
{
  // Can alternatively use std::chrono::system_clock
  const auto start_time = std::chrono::steady_clock::now();
  for (;;)
  {
    const auto now = std::chrono::steady_clock::now();
    const double epoch_s = std::chrono::duration<double>>(now - start_time).count();
    scheduler.update(epoch_s);
    std::this_thread::sleep_until(now + std::chrono::milliseconds(1000 / 60));
  }
}
```

This runs the `Scheduler` in "real time" using the system clock. However, the `Scheduler`
deliberately uses an external time source so that the caller has control over the clock. This allows
the caller to run time at a slower or faster than real time rate, or to run with zero delta updates.

Fibres can either infer the progress of time by sleeping for known durations (though this is
imprecise), or by having access to the `Scheduler` object and accessing`Scheduler::time().dt` or
`Scheduler::time().epoch_time_s`. This `Scheduler::time()` is updated at the start of each
`update()` call.

Conversely the `ThreadPool` always runs in real time using `std::chrono::stead_clock`.

## Yielding and waiting

It is critical that each fibre periodically suspend to avoid starving other fibres or the calling
thread. This is done by either a `co_yield` or `co_await` statement. These statements are somewhat
interchangeable in the Morai library, but have different semantics. A `co_yield` always suspends
until at least the next update cycle, while a `co_await` suspends control only if the wait condition
is unmet.

The following `co_yield` patterns are supported:

- `co_yield {};`
  - Default yield, suspending until the next update.
- `co_yield morai::yield();`
  - Equivalent to `co_yield {};`, effectively an alias.
- `co_yield moral::sleep(duration);`
  - Suspend until the specified duration has elapsed in the epoch time. Duration may be a `double`
    or `float` seconds value or a `std::chrono::duration`.
- `co_yield morai::wait(condition, timeout);`
  - Suspend until the `condition` returns true, or the `timeout` elapses (zero/negative for no
    timeout).
    - `condition` is a callable object - `bool f()`.
    - `timeout` (optional) may be a `double`, `float` or `std::chrono::duration` as per sleep.
    - Be sure to validate the `condition` on resume if a `timeout` was specified. This will still be
      false if the timeout has elapsed.

The following `co_await` patterns are supported:

- `co_await <yield_condition>;`
  - Await any of the `co_yield` values. Equivalent to `co_yield` in most cases, except for the
    `wait()` conditions. `wait()` does not suspend if the condition is already met.
- `co_await <double>;`, `co_await <float>;`, `co_await <std::chrono::duration>;`
  - Wait for the specified epoch time to have elapsed.
  - Alternative expression for sleeping.
- `co_await <morai::WaitCondition>;`
  - Wait until the `morai::WaitCondition` callable returns true. Similar to `morai::wait()` except
    that a lambda expression can be directly waited on - e.g.;
    - `co_await []() { return true; };`
- `co_await <morai::Id>;`
  - Suspend until the `Fibre` with the given `Id` has finished.
  - Beware of deadlocks.
- `co_await morai::reschedule(priority[, position]);`
  - Resume after moving the fibre to a different priority level in the current scheduler.
  - See [rescheduling](#priority-and-rescheduling-fibres).
- `co_await morai::moveTo(scheduler[, priority]);`
  - Resume after moving this fibre to a new `scheduler`, optionally at a new `priority`.
  - See [moving fibres](#moving-fibres-between-schedulers)

## Cancelling fibres

A fibre may be cancelled via its scheduler if managed by a `Scheduler` object (not `ThreadPool`) or via the `Id` calling
`Id::markForCancellation()`. The former immediately cancels the former,  provided the `Scheduler` is managing the fibre,
while the latter sees the fibre terminates the next time it would resume (without resuming).

## Awaiting other fibres

A `Fibre` can suspend until another `Fibre` completes using by using `co_await` with the second
fibre's `Id` object. This `Id` is returned when the fibre is added to a scheduler - e.g.,
`Scheduler::start()` or `ThreadPool::start()` - and remains unique to that fibre. The `Id` object
is a lightweight wrapper around a shared ID value. The `Id` object can be used to see if a fibre
is still running - `Id::running()`. This value is true as soon as the `Fibre` is created and
becomes false after the `Fibre` is cleaned up.

The `co_wait <Id>;` statement essentially creates a wait condition on `!Id::running()`.

Be wary of having fibres wait on other fibres as it is very easy to create circular dependencies
and deadlock all the waiting fibres. A fibre waiting on itself will also deadlock.

Waiting on an invalid `Id` - where the value is `morai::InvalidFibreValue` - always immediately
returns from the `co_await` statement, never suspending the fibre.

## Priority and rescheduling fibres

The `Scheduler` and `ThreadPool` support creating a set of fixed priority value queues on creation.
Lower priority value queues are updated first, thus lower priority values are considered higher in
priority. When a fibre starts, it is assigned to the priority queue that best matches the fibre
priority using a lower bounding method. That is, it is assigned to the highest priority queue it
can be assigned to.

A fibre may use `co_await morai::reschedule(priority);` statement to adjust its priority. This moves
the fibre to the priority queue matching the new priority value and resumes when that priority queue
next updates.

During a `Scheduler::update()`, each queue is drained until all queues have been cycled once, from
highest priority (lowest value) to lowest. For the single threaded `Scheduler`, this means that a
`Fibre` can be updated multiple times in a single `update()` cycle if it is continually moved to a
lower priority queue.

The `ThreadPool` scheduler worker threads treat priority a little differently. Essentially each
worker runs a loop in which it pops a `Fibre`, updates it, replaces it on its queue, then pops a
new `Fibre`. The higher priority queues are essentially checked for fibres more often than lower
priority queues. Given four queues, a worker will check the highest priority queue four times, the
next queue three, then two and the last once to complete a cycle. There is no consideration if a
fibre has already been updated, so lower priority queues have far fewer updated.

## Moving fibres between schedulers

Fibres can be moved between schedulers so long as the fibre has a pointer to the target scheduler.
This is done using a `co_await morai::moveTo();` statement. The code below shows a fibre that
continually swaps between two schedulers.

```c++
morai::Fibre pingPong(morai::Scheduler *ping, morai::Scheduler *pong)
{
  for (;;)
  {
    co_await morai::moveTo(ping);
    std::cout << "ping\n";
    co_await morai::moveTo(pong);
    std::cout << "pong\n";
  }
}
```

On a move operation, the fibre is suspended, moved to the new scheduler by calling its `move()`
method, and resumed once the new scheduler updates.

All Morai schedulers use a thread safe queue to accept incoming fibres, so moving between threads is
allowed. This allows fibre code to be written without explicit thread synchronisation while being
effectively multi-threaded code. The move operation acts as an implicit synchronisation point.

It is possible for a move operation to fail because the `morai::SharedQueue` - the threadsafe
`Fibre` queue - is fixed size. The target queue may be full. In this case the `Fibre` remains with
the current scheduler and another move attempt is made when it is next popped. The fibre will still
only resume once it is in the new scheduler, but this may take longer.

## Things to do with fibres

- Spawning fibres from fibres
- Waiting on fibres
- Rescheduling
- Moving between threads.

## Pitfalls

- Lifetime and ownership - passing or capturing references
- Avoid capturing by value in lambda expression fibres. Prefer arguments - see example below.
  - See also the fibre test `Fibre.capture`.

```c++
#include <morai/Scheduler.hpp>

#include <iostream>

int main()
{
  using namespace morai;
  scheduler.start(
    [](std::shared_ptr<int> state) -> Fibre {
      // State variable is definitely copied into the fibre frame and remains valid for the life
      // of the fibre.
      std::cout << "Better: " << *state << std::endl;
      co_return;
    }(state),
    "Better");

  scheduler.start(
    [&state]() -> Fibre {
      // State likely to be valid once the fibre starts, by may become invalid as the fibre
      // continues.
      std::cout << "Risky: " << *state << std::endl;
      co_return;
    }(),
    "Risky");

  scheduler.start(
    [state]() -> Fibre {
      // Buy the time the fibre starts, the state variable may have been disposed.
      // This seems to come about if the fibre is moved before it starts.
      std::cout << "Bad: " << *state << std::endl;
      co_return;
    }(),
    "Bad");

  scheduler.update();
}
```
