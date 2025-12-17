# Morai overview

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
exits. The `main()` function creates a scheduler for this fibre and runs it.

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

/*
Output:
Hello
world
*/
```

A Fibre can be thought of a kind of state machine where the current state is inherently embedded in
the function arguments, the local variables, and the program counter. Fibres offer a simpler way of
writing logic that depends on other things occurring at some point in time, where it is now well
known when or if that other thing will occur.

For example, consider a `Turret` game object, written as a C++ class.

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

This code assumes the `Turret::update()` function is called regularly and is re-entrant. The
`update()` function must determine the current state of the object to determine the appropriate
logic to perform, evaluate external state, then initiate new effects. This becomes more complicated
as we attempt to track multiple states in parallel, such as making turret alignment independent of
firing.

Below is a fibre based example. Note is how easily we can separate various parts of the logic into
isolated responsibilities. We have reduced nesting and the code for each fibre flows more naturally.
Each fibre defines its own state machine where the program counter helps track progress through the
states.

```c++
struct Turret
{
  Vec3 facing{};    ///< Current forward facing vector
  Vec3 target{};    ///< Current target point
  std::vector<morai::Id> fibres;  ///< List of running fibre IDs for the object
  int health = 100; ///< Current health points.
};

morai::Fibre Turret::deadWatchFibre(std::shared_ptr<Turret> turret)
{
  // Track 
  co_await [this]() { return turret->health <= 0; };
  cancelFibres(turret->fibres);
}

morai::Fibre Turret::trackingFibre(std::shared_ptr<Turret> turret)
{
  for (;;)
  {
    auto target = getBestTarget(*turret);
    turnTo(turret->facing, target, getDt());
    co_yield {};
  }
}

morai::Fibre Turret::aimAndFireFibre(std::shared_ptr<Turret> turret)
{
  for (;;)
  {
    co_await [this] () { return aligned(turret->target); }
    fireAt(turret->target, dt);
    co_yield {};  // Ensure we yield between firing. co_await may not need to suspend.
  }
}
```

## Yielding and waiting

It is critical that each fibre periodically suspend to avoid starving other fibres or the calling
thread. This is done by either a `co_yield` or `co_await` statement. These statements are somewhat
interchangeable in the Morai library, but have different semantics. A `co_yield` always suspends
until at least the next update cycle, while a `co_await` suspends control only if the wait condition
is unmet.

The following `co_yield` patterns are supported:

- `co_yield {};`
  - Default yield, suspending until the next scheduled update.
- `co_yield morai::yield();`
  - Equivalent to `co_yield {};` - effectively an alias.
- `co_yield moral::sleep(duration);`
  - Suspend until the specified duration has elapsed in the epoch time. Duration type supports
    `double` or `float` seconds values or a `std::chrono::duration`.
- `co_yield morai::wait(condition, timeout);`
  - Suspend until the `condition` returns true, or the (optional) `timeout` elapses (zero/negative
    for no timeout).
    - `condition` is a callable object - `bool f()`.
    - `timeout` (optional) may be a `double`, `float` or `std::chrono::duration` as per sleep.
    - You may need to validate the `condition` on resume when a `timeout` was specified. The
      condition may still be false once the timeout has elapsed.

The following `co_await` patterns are supported:

- `co_await <yield_condition>;`
  - Await any of the `co_yield` values. Equivalent to `co_yield` in most cases, except that the
    fibre will not suspend if the yield state is already met - e.g., awaiting zero seconds does not
    suspend, neither does `wait()` on an already met condition.
- `co_await <double>;`, `co_await <float>;`, `co_await <std::chrono::duration>;`
  - Wait for the specified epoch time to have elapsed.
  - Alternative expression for sleeping.
- `co_await <morai::WaitCondition>;`
  - Wait until the `morai::WaitCondition` callable returns true. Similar to `morai::wait()` except
    that a lambda expression can be directly waited on - e.g.;
    - `co_await []() { return true; };` vs `co_await wait([]() { return true; });`
- `co_await <morai::Id>;`
  - Suspend until the `Fibre` with the given `Id` has finished.
  - Beware of deadlocks.
- `co_await morai::reschedule(priority[, position]);`
  - Resume after moving the fibre to a different priority level in the current scheduler.
  - See [rescheduling](#priority-and-rescheduling-fibres).
- `co_await morai::moveTo(scheduler[, priority]);`
  - Resume after moving this fibre to a new `scheduler`, optionally at a new `priority`.
  - See [moving fibres](#moving-fibres-between-schedulers)

## Awaiting other fibres

A `Fibre` can suspend until another `Fibre` completes using by using `co_await` with a `morai::Id`
object identifying the fibre to wait for. A fibre `Id` is returned when the fibre is added to a
scheduler - e.g., `Scheduler::start()` or `ThreadPool::start()` - and remains unique to that fibre.
The `Id` object is a lightweight wrapper around a shared ID value. The `Id` object can be used to
see if a fibre is still running - `Id::running()`. The running value is true as soon as the `Fibre`
is created and becomes false after the `Fibre` is cleaned up.

The `co_wait <Id>;` statement essentially creates a wait condition on `!Id::running()`.

Be wary of having fibres wait on other fibres as it is very easy to create circular dependencies
and deadlock all the waiting fibres. A fibre waiting on itself will also deadlock. The API does
not provide a way of finding the currently running fibre's ID.

An invalid `Id` object is never considered to be running, thus waiting on an invalid `Id` - where
the value is `morai::InvalidFibreValue` - always immediately returns from the `co_await` statement,
never suspending the fibre.

## Cancelling fibres

A fibre may be cancelled via its `Id` object - `Id::markForCancellation()`. This flags the fibre to
be cancelled the next time it is scheduled for resume. Alternatively `Scheduler::cancel()` may be
used to immediately cancel a fibre that is running in that scheduler. There is no equivalent
cancellation function for the `ThreadPool` scheduler. In either case, the target fibre is never
resumed again.

## Fibre cleanup

C++ coroutine functions are unwound like any other C++ scope. This means that a fibre's local
variables are all destroyed and destructed when the fibre is destroyed. This makes fibres
[RAII](https://en.cppreference.com/w/cpp/language/raii.html) safe.

This RAII safety can be used to effect a pseudo destructor in a `Fibre` using `gsl::finally()` or
`morai::finally()` (the Morai version is a copy of the [GSL](https://github.com/microsoft/GSL)
version). The example below shows how to use how `finally()` callbacks are always invoked.

```c++
morai::Fibre pseudoDestructor()
{
  const auto pseudo_destruct = morai::finally([]() {
    std::cout << "Goodbye fibre\n";
  });

  co_yield std::chrono::seconds(5);
}

// After 5s of running, the fibre is cleaned up and we see the output:
// Goodbye fibre
```

## Schedulers

There are two schedulers in the core Morai library - `Scheduler` and `ThreadPool`. The `Scheduler`
is a single threaded scheduler, while the `TheadPool` is a threadsafe, multi-threaded worker based
scheduler. The table below shows some different features of the two schedulers.

| Feature                 | `Scheduler`           | `ThreadPool`                   |
| ----------------------- | --------------------- | ------------------------------ |
| **Threading**           | Single threaded       | Multiple worker threads        |
| **Priority scheduling** | fixed priority values | fixed priority values          |
| **Queue sizing**        | Growable              | Fixed size                     |
| **Start fibres**        | `start()`             | `start()`                      |
| **Cancel fibre by Id**  | yes                   | no                             |
| **Cancel all**          | yes                   | yes                            |
| **Move to**             | yes - threadsafe      | yes - threadsafe               |
| **Threadsafe update**   | no                    | yes                            |
| **Explicit update**     | only                  | optional, time sliced          |
| **External threads**    | N/A                   | optional - explicit `update()` |

See scheduler class documentation for further details.

## Epoch time

The default `Scheduler` class is a single threaded fibre scheduler that attempts to resume all
fibres during a `Scheduler::update()` call. The class is constructed with a `Clock` object that is
used to track epoch time to support duration dependent logic, such as sleeping. The default `Clock`
uses `std::chrono::steady_clock` for its epoch time, thus operates in real time. Alternatively the
`Clock::timeFunction()` may be replaced with a custom function for user defined time evolution. The
reported time value must be monotonic, but may the rate may vary.

Fibres can either infer the progress of time by sleeping for known durations (though this is
imprecise), or by having access to the `Scheduler` object and accessing`Scheduler::time().dt` or
`Scheduler::time().epoch_time_s`. This `Scheduler::time()` is updated at the start of each
`update()` call.

The `ThreadPool` scheduler also supports the `Clock` interface, but does not otherwise expose the
epoch time.

## Priority and rescheduling fibres

The `Scheduler` and `ThreadPool` constructors support specifying a set of fixed priority value on
creation. A scheduling queue is created for each priority value. Lower priority value queues are
updated first, thus lower priority values have higher precedence. All scheduled fibres are assigned
to the priority queue that best matches the fibre priority using a lower bounding method. That is,
a fibre is assigned to the highest priority queue (lowest value) it can be assigned to.

A fibre may use a `co_await morai::reschedule(priority);` statement to adjust its priority. This
suspends the fibre, then moves it to the priority queue matching the new priority value. The fibre
resumes when that priority queue next updates.

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

Fibres can be moved between schedulers so long as the fibre has a pointer or reference to the target
scheduler. This is done using a `co_await morai::moveTo();` statement. The code below shows a fibre
that continually swaps between two schedulers.

```c++
morai::Fibre pingPong(
    std::shared_ptr<morai::Scheduler> ping,
    std::shared_ptr<morai::Scheduler> pong
  )
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

On a move operation, the fibre is suspended, moved to the new scheduler by calling the scheduler
`move()` function, then the fibre resumes once the new scheduler updates.

All Morai schedulers use a thread safe queue to accept incoming fibres, so moving between threads is
allowed. This supports fibre code to be written without explicit thread synchronisation while being
effectively multi-threaded code. The move operation acts as an implicit synchronisation point.

It is possible for a move operation to fail because the `morai::SharedQueue` is full - the
threadsafe `Fibre` queue is fixed size. In this case the `Fibre` remains with the current scheduler
and another move attempt is made on the next update. The fibre remains suspended until it is moved
to the new scheduler and only the new scheduler may resume the fibre, but this may take longer to
effect.

## Other things to do with fibres

- Spawning fibres from fibres is supported in all `morai` schedulers.

## Pitfalls

Be mindful of lifetime and ownership. A fibre function has a longer lifetime than a standard
function, this any pointers or references taken by the fibre must outlive the fibre, or the fibre
must participate in ownership - i.e., use `std::shared_ptr`.

There is also a potential C++ coroutine bug in writing fibres that use lambda capture *by value*.
Fibres start suspended, thus they do not immediately access the lambda captured variables. By the
time the fibre resumes, the lambda variables may no longer be valid. This seems to be less of an
issue when capturing *by reference*, but caution is advised. It is better to pass arguments to the
fibre function and avoid all lambda captures. This is shown in the example below - see also the
fibre library test `Fibre.capture`.

```c++
#include <morai/Scheduler.hpp>

#include <iostream>

int main()
{
  using namespace morai;
  scheduler.start(
    [](std::shared_ptr<int> state) -> Fibre {
      // State variable is copied into the fibre frame on start and remains valid for the life of
      // the fibre.
      std::cout << "Ok: " << *state << std::endl;
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
      // The state variable is invalid by the time the fibre starts and the program will crash.
      std::cout << "Bad: " << *state << std::endl;
      co_return;
    }(),
    "Bad");

  scheduler.update();
}
```
