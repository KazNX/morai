#pragma once

#include "Id.hpp"
#include "Common.hpp"
#include "Resumption.hpp"

#include <atomic>
#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace arachne
{
namespace detail
{
struct Frame
{
  /// Indicates when next to resume the fibre - see @c Resumption. Note the time value is
  /// initially given as a relative time, but stored as an epoch time.
  Resumption resumption{};
  std::optional<Priority> reschedule{};
  std::exception_ptr exception{};  ///< Exception storage.
  Id id{};
  int32_t priority = 0;
  std::string name;
  bool cancel = true;
};
}  // namespace detail

/// The @c Fibre implements a coroutine interface for the fibre system. It tracks the current fibre
/// state and supports resuming.
///
/// The fibre interface supports the following coroutine suspension mechanisms:
///
/// - `co_yield {};` - resume next update.
/// - `co_yield <double>;` - resume after the specified number of seconds.
/// - `co_await sleep(duration_s);` - resume after duration_s seconds.
/// - `co_await std::chrono::duration{x}` - resume after the specified duration.
/// - `co_await wait(condition);` - resume when condition() returns true.
/// - `co_await []() -> bool { ... };` - resume when the lambda returns true.
/// - `co_await wait(condition, timeout_s);` - resume when condition() returns true or after
///   timeout_s seconds.
/// - `co_return;` - end fibre execution.
class Fibre
{
public:
  struct promise_type;

  /// Implements the awaitable interface for fibre coroutines.
  struct Awaitable
  {
    /// Transient storage for resumption condition. Propagated to the promise on suspension.
    Resumption resumption;
    /// Check if the fibre can immediately continue (true) or fibre needs to suspend (false).
    bool await_ready() const { return resumption.condition && resumption.condition(); }
    /// Suspend the fibre, migrating the @c resumption condition to the promise.
    void await_suspend(std::coroutine_handle<promise_type> handle) noexcept;
    /// Resumption handling - no-op.
    void await_resume() noexcept {}
  };

  struct FibreIdAwaitable
  {
    /// Transient storage for resumption condition. Propagated to the promise on suspension.
    Id id;
    /// Check if the fibre can immediately continue (true) or fibre needs to suspend (false).
    bool await_ready() const { return !id.isRunning(); }
    /// Suspend the fibre, migrating the @c resumption condition to the promise.
    void await_suspend(std::coroutine_handle<promise_type> handle) noexcept;
    /// Resumption handling - no-op.
    void await_resume() noexcept {}
  };

  struct RescheduleAwaitable
  {
    Priority value;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<promise_type> handle) noexcept;
    void await_resume() noexcept {}
  };

  template <typename Scheduler>
    requires SchedulerType<Scheduler>
  struct MoveAwaitable
  {
    Scheduler *target = nullptr;
    bool await_ready() const noexcept { return target == nullptr; }
    void await_suspend(std::coroutine_handle<Fibre::promise_type> handle) noexcept;
    void await_resume() noexcept {}
  };

  /// Fibre @c promise_type implementation.
  struct promise_type
  {
    detail::Frame frame{};

    /// Convert to the owning @c Fibre object.
    Fibre get_return_object() noexcept
    {
      return Fibre{ std::coroutine_handle<promise_type>::from_promise(*this),
                    Id{ Fibre::nextId() } };
    }

    /// Initial suspension - always.
    std::suspend_always initial_suspend() noexcept { return {}; }
    /// Final suspension - always.
    std::suspend_always final_suspend() noexcept { return {}; }
    /// Exception handling - store to be rethrown on @c Fibre::resume().
    void unhandled_exception() noexcept { frame.exception = std::current_exception(); }

    /// Return type definition - void.
    void return_void() noexcept {}

    /// Yield type definition - @c Resumption.
    /// @param value Immediately stored into the @c resumption member (time still relative).
    std::suspend_always yield_value(Resumption &&value) noexcept
    {
      frame.resumption = std::move(value);
      return {};
    }

    /// @c co_await handling for @c double sleep durations.
    /// @param duration_s The time to sleep (seconds).
    Awaitable await_transform(const double duration_s) noexcept
    {
      return { .resumption = sleep(duration_s) };
    }

    /// @c co_await handling for std::chrono::duration sleep durations.
    /// @param duration The duration to sleep for.
    template <typename Rep, typename Period>
    Awaitable await_transform(const typename std::chrono::duration<Rep, Period> &duration) noexcept
    {
      return { .resumption = sleep(duration) };
    }
    /// General @c co_await handling for @c Resumption types. Handles @c sleep(), @c wait() or
    /// @c resumption().
    /// @param resumption The the resumption condition.
    Awaitable await_transform(const Resumption &resumption) noexcept
    {
      return { .resumption = resumption };
    }
    /// @c co_await handling for @c WaitCondition callbacks. This includes lambda expression
    /// handling.
    /// @param condition The callable wait condition. Resume when it returns @c true.
    Awaitable await_transform(WaitCondition condition) noexcept
    {
      return { .resumption = wait(condition) };
    }

    /// @c co_await a fibre @c Id. Waits until the @c Id is flagged as not running.
    FibreIdAwaitable await_transform(const Id &id)
    {
      //
      return { .id = id };
    }

    /// @c co_await handling for @c Priority rescheduling.
    /// Waits for the fibre to be assigned a new priority. Essentially this resumes on the next
    /// update.
    RescheduleAwaitable await_transform(Priority &&reschedule) noexcept
    {
      return { .value = std::move(reschedule) };
    }

    template <typename Scheduler>
      requires SchedulerType<Scheduler>
    MoveAwaitable<Scheduler> await_transform(Scheduler *move_to)
    {
      return { .target = move_to };
    }
  };

  Fibre() = default;
  Fibre(std::coroutine_handle<promise_type> handle, Id id)
    : _handle{ handle }
  {
    auto &promise = _handle.promise();
    id.setRunning(true);
    promise.frame.id = std::move(id);
    promise.frame.cancel = false;
  }
  Fibre(std::coroutine_handle<promise_type> handle)
    : _handle{ handle }
  {}
  Fibre(const Fibre &) = delete;
  Fibre(Fibre &&other) noexcept
    : _handle{ std::exchange(other._handle, {}) }
  {}
  Fibre &operator=(const Fibre &) = delete;
  Fibre &operator=(Fibre &&other) noexcept
  {
    swap(other);
    return *this;
  }

  ~Fibre();

  [[nodiscard]] Id id() const { return { (_handle) ? _handle.promise().frame.id : Id{} }; }

  [[nodiscard]] std::string_view name() const
  {
    return (_handle) ? _handle.promise().frame.name : std::string_view{};
  }
  void setName(std::string_view name) { _handle.promise().frame.name = name; }

  [[nodiscard]] bool cancel() const { return (_handle) ? _handle.promise().frame.cancel : true; }
  void markForCancellation()
  {
    if (_handle)
    {
      _handle.promise().frame.cancel = true;
    }
  }

  [[nodiscard]] int32_t priority() const
  {
    return (_handle) ? _handle.promise().frame.priority : 0;
  }

  void __setPriority(int32_t p)
  {
    if (_handle)
    {
      _handle.promise().frame.priority = p;
    }
  }

  /// Checks if this is a valid fibre.
  [[nodiscard]] bool valid() const noexcept
  {
    return _handle && _handle.promise().frame.id.valid();
  }

  /// Check if the fibre has completed execution.
  [[nodiscard]] bool done() const noexcept
  {
    return !_handle || _handle.done() || _handle.promise().frame.cancel;
  }

  /// Attempt to resume fibre execution.
  ///
  /// This returns control to the fibre coroutine so long as the @c promise_type::resumption
  /// conditions are met. There are two conditions which may be met:
  ///
  /// - @p epoch_time_s is greater than or equal to @c Resumption::time_s and there is no
  ///   @c Resumption::condition.
  /// - There is a @c Resumption::condition and it has been met (returns @c true ), or the condition
  ///   returns @c false, the @c Resumption::time_s is greater than zero and the @p epoch_time_s is
  ///   greater than or equal to @c Resumption::time_s.
  ///
  /// Resuming the coroutine sets a new @p promise_time::resumption value via either a @c co_yield
  /// or
  /// @c co_await. This new @c Resumption has a relative @c time_s value, which is converted to an
  /// epoch time before returning. No @c Resumption object is given when the fibre completes  -
  /// @c co_return.
  ///
  /// @param epoch_time_s The current epoch time in seconds as given to @c Scheduler::update().
  /// @return The new fibre state, which tells the @c Scheduler what to do with next with this
  /// @c Fibre.
  [[nodiscard]] Resume resume(const double epoch_time_s) noexcept;

  std::exception_ptr exception() const noexcept
  {
    return _handle ? _handle.promise().frame.exception : nullptr;
  }

  void swap(Fibre &other) noexcept { std::swap(_handle, other._handle); }

  [[nodiscard]] static IdValueType nextId()
  {
    // We increment by 2 to avoid the running bit.
    constexpr uint64_t increment = 2;
    IdValueType id = (_next_id += increment);
    if ((id | Id::RunningBit) == InvalidFibreValue) [[unlikely]]
    {
      id = (_next_id += increment);
    }
    return id;
  }

private:
  void flagNotRunning() noexcept { _handle.promise().frame.id.setRunning(false); }

  std::coroutine_handle<promise_type> _handle;
  static std::atomic<IdValueType> _next_id;
};


template <typename Scheduler>
  requires SchedulerType<Scheduler>
void Fibre::MoveAwaitable<Scheduler>::await_suspend(
  std::coroutine_handle<Fibre::promise_type> handle) noexcept
{
  if (target)
  {
    target->move(Fibre{ std::move(handle) });
    handle.promise().frame.cancel = true;
  }
  target = nullptr;
}
}  // namespace arachne


namespace std
{
template <>
struct less<arachne::Fibre>
{
  bool operator()(const arachne::Fibre &a, const arachne::Fibre &b) const
  {
    return a.priority() < b.priority();
  }
};

template <>
inline void swap(arachne::Fibre &a, arachne::Fibre &b) noexcept
{
  a.swap(b);
}
}  // namespace std
