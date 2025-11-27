#pragma once

#include "Id.hpp"
#include "Common.hpp"
#include "Move.hpp"
#include "Resumption.hpp"

#include <atomic>
#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace morai
{
class Fibre;

namespace detail
{
/// Internal fibre data - stored in the @c Fibre::promise_type.
struct Frame
{
  /// Indicates when next to resume the fibre - see @c Resumption. Note the time value is
  /// initially given as a relative time, but stored as an epoch time.
  Resumption resumption{};
  /// Set to a new target priority when priority rescheduling is requested.
  std::optional<Priority> reschedule{};
  std::exception_ptr exception{};  ///< Exception storage.
  /// Unique Id of this fibre.
  Id id{};
  /// Current fibre priority.
  int32_t priority = 0;
  /// Optional fibre name - debug info only.
  std::string name;
  /// Set when a request to move to another scheduler is made. See @c moveTo(). Cleared after the
  /// moved.
  std::function<bool(Fibre &)> move_operation{};
};
}  // namespace detail

/// The @c Fibre implements a coroutine interface for the fibre system. It tracks the current fibre
/// state and supports resuming.
///
/// A valid @c Fibre function has the following requirements:
///
/// - Has a return type of @c morai::Fibre
/// - Uses C++ coroutine features - @c co_await, @c co_yield and/or @c co_return.
/// - Must periodically cede control via @c co_await or @c co_yield to avoid starving other fibres
///   or the calling thread.
///
/// The fibre interface supports the following coroutine suspension mechanisms:
///
/// - `co_yield {};` - resume next update.
/// - `co_yield yield();` - equivalent to `co_yield {};` - see @c yield().
/// - `co_yield/co_await sleep(duration);` - sleep for the specified epoch duration
///   - See @c sleep()
/// - `co_yield/co_await wait(condition[, timeout_s]);` - resume once the condition returns true or
///   after the @c timeout_s has elpased the optional timeout expires
///   - See @c wait()
/// - `co_await <double>;` - resume after the specified number of seconds of epoch time.
/// - `co_await std::chrono::duration{x}` - resume after the specified duration of epoch time.
/// - `co_await WaitCondition{};` - resume when the @c WaitCondition function returns true.
/// - `co_await []() -> bool { ... };` - resume when the lambda returns true.
/// - `co_await reschedule(priority[, position]);` - reschedule the fibre at the given priority.
///  - See @c reschedule()
/// - `co_await <Id>;` - resume after the fibre with the given @c Id is no longer running.
/// - `co_await moveTo(scheduler[, priority]);` - move the fibre to another scheduler, optionally
///   at a new priority.
/// - `co_return;` - end fibre execution.
class Fibre
{
public:
  struct promise_type;

  /// Implements the awaitable interface for @c Resumption types - i.e., general @c co_await
  /// handling.
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

  /// Implements the awaitable interface for @c Id types - i.e., @c co_await another fibre.
  struct FibreIdAwaitable
  {
    /// Transient storage for resumption condition. Propagated to the promise on suspension.
    Id id;
    /// Check if the fibre can immediately continue (true) or fibre needs to suspend (false).
    bool await_ready() const { return !id.running(); }
    /// Suspend the fibre, migrating the @c resumption condition to the promise.
    void await_suspend(std::coroutine_handle<promise_type> handle) noexcept;
    /// Resumption handling - no-op.
    void await_resume() noexcept {}
  };

  /// Implements the awaitable interface for @c Priority rescheduling - i.e., @c co_await
  /// @c reschedule().
  struct RescheduleAwaitable
  {
    /// New priority value storage.
    Priority value;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<promise_type> handle) noexcept;
    void await_resume() noexcept {}
  };

  /// Implements the awaitable interface for @c MoveTo - i.e., @c co_await @c moveTo().
  template <typename Scheduler>
    requires SchedulerType<Scheduler>
  struct MoveAwaitable
  {
    MoveTo<Scheduler> move{};
    bool await_ready() const noexcept { return move.target == nullptr; }
    void await_suspend(std::coroutine_handle<Fibre::promise_type> handle) noexcept;
    void await_resume() noexcept {}
  };

  /// Fibre @c promise_type implementation.
  struct promise_type
  {
    detail::Frame frame{};

    /// Destructor - marks the @c Fibre @c Id as no longer running.
    ~promise_type() { frame.id.setRunning(false); }

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
    FibreIdAwaitable await_transform(const Id &id) { return { .id = id }; }

    /// @c co_await handling for @c Priority rescheduling.
    /// Waits for the fibre to be assigned a new priority. Essentially this resumes on the next
    /// update.
    RescheduleAwaitable await_transform(Priority &&reschedule) noexcept
    {
      return { .value = std::move(reschedule) };
    }

    template <typename Scheduler>
      requires SchedulerType<Scheduler>
    MoveAwaitable<Scheduler> await_transform(MoveTo<Scheduler> move_to)
    {
      return { .move = move_to };
    }
  };

  /// Create an empty, invalid fibre.
  Fibre() = default;
  /// Create a fiber around the given coroutine @p handle and @p Id.
  Fibre(std::coroutine_handle<promise_type> handle, Id id)
    : _handle{ handle }
  {
    auto &promise = _handle.promise();
    id.setRunning(true);
    promise.frame.id = std::move(id);
  }
  /// Create a fibre around the given coroutine @p handle - preserves the current @c Id. This should
  /// only be called for fibres after a @c __release().
  Fibre(std::coroutine_handle<promise_type> handle)
    : _handle{ handle }
  {}

  Fibre(Fibre &&other) noexcept
    : _handle{ std::exchange(other._handle, {}) }
  {}

  /// Move assignment - self move is supported.
  Fibre &operator=(Fibre &&other) noexcept
  {
    swap(other);
    return *this;
  }

  Fibre(const Fibre &) = delete;
  Fibre &operator=(const Fibre &) = delete;

  /// Destructor - cleans up the coroutine.
  ~Fibre();

  [[nodiscard]] Id id() const { return { (_handle) ? _handle.promise().frame.id : Id{} }; }

  [[nodiscard]] std::string_view name() const
  {
    return (_handle) ? _handle.promise().frame.name : std::string_view{};
  }
  /// Set the fiber (debug) name.
  void setName(std::string_view name) { _handle.promise().frame.name = name; }

  /// Get the fibre scheduling priority.
  [[nodiscard]] int32_t priority() const
  {
    return (_handle) ? _handle.promise().frame.priority : 0;
  }

  /// Sets the fibre scheduling priority. For internal use only. Use @c Scheduler::start() and
  /// `co_await reschedule();` to set user priority levels.
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
  [[nodiscard]] bool done() const noexcept { return !_handle || _handle.done(); }

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

  /// Get any exception raised during fibre execution.
  std::exception_ptr exception() const noexcept
  {
    return _handle ? _handle.promise().frame.exception : nullptr;
  }

  /// Swap contents of this fiber with another - self swap supported.
  void swap(Fibre &other) noexcept { std::swap(_handle, other._handle); }

  /// Generate the next unique fibre Id value.
  [[nodiscard]] static IdValueType nextId()
  {
    IdValueType id = (_next_id += Id::Increment);
    if ((id | Id::SpecialBits) == InvalidFibreValue) [[unlikely]]
    {
      id = (_next_id += Id::Increment);
    }
    return id;
  }

  /// Release ownership of the internal coroutine handle. The caller is then responsible for
  /// calling @c handle.destroy() . For internal use only.
  std::coroutine_handle<promise_type> __release() { return std::exchange(_handle, {}); }
  /// Get the internal coroutine handle. For internal use only.
  std::coroutine_handle<promise_type> __handle() { return _handle; }

private:
  std::coroutine_handle<promise_type> _handle;
  static std::atomic<IdValueType> _next_id;
};


template <typename Scheduler>
  requires SchedulerType<Scheduler>
void Fibre::MoveAwaitable<Scheduler>::await_suspend(
  std::coroutine_handle<Fibre::promise_type> handle) noexcept
{
  if (move.target)
  {
    // Setup the move operation.
    handle.promise().frame.move_operation = [move = this->move](Fibre &fibre) -> bool {
      return move.target->move(fibre, move.priority);
    };
    move.target = nullptr;
  }
}
}  // namespace morai


namespace std
{
/// Specialisation of @c std::less for @c morai::Fibre to allow use in ordered containers.
template <>
struct less<morai::Fibre>
{
  bool operator()(const morai::Fibre &a, const morai::Fibre &b) const
  {
    return a.priority() < b.priority();
  }
};

/// Specialisation of @c std::swap for @c morai::Fibre.
template <>
inline void swap(morai::Fibre &a, morai::Fibre &b) noexcept
{
  a.swap(b);
}
}  // namespace std
