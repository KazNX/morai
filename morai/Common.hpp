#pragma once

#include <cstdint>
#include <functional>
#include <optional>

namespace morai
{
/// Shared parameters for creating a @c Scheduler.
struct SchedulerParams
{
  /// Initial fibre queue size. This may grow (double) as required.
  uint32_t initial_queue_size = 1024u;
  /// Size of the threadsafe move queue used for @c moveTo() operations. This is a fixed size queue
  /// and fails move operations once full.
  uint32_t move_queue_size = 1024u;
  /// List of supported priority levels. One queue is created for each level at the
  /// @c initial_queue_size. The levels are sorted (ascending) before creating queues, but duplicate
  /// values yield undefined behaviour.
  std::vector<int32_t> priority_levels{};
};

enum class ExceptionHandling
{
  /// Log exceptions and continue running.
  Log,
  /// Rethrow exceptions to the caller of @c Scheduler::update().
  Rethrow,
};

/// Time structure used by @c Scheduler to track time.
struct Time
{
  /// Total epoch time. This is user defined. See @c Scheduler::update().
  double epoch_time_s = 0.0;
  /// Delta time since the last update.
  double dt = 0.0;
};

/// Function signature used for wait conditions - i.e., `co_await <condition>;` statements.
/// See @c Fibre @c co_await handling.
///
/// @return True once the fibre may resume.
using WaitCondition = std::function<bool()>;

class Fibre;

/// Concept required to for supporting `co_await morai::moveTo()` operations. A fibre can be moved
/// to any object that supports the function signature:
///
/// - `bool move(Fibre &fibre, std::optional<int> priority);`
///
/// Implementations of hte @c move() have the following responsibilities:
///

/// 1. On success, invalidate the @p fibre argument by calling @c fibre.__release()
/// 2. On success, set the fibre priority, typically by directly modifying
///   @c Fibre::promise_type::frame::priority
/// 3. On failure, leave the @p fibre in a valid state - do not @c __release().
/// 4. Return true on success, false on failure.
///
/// @code
/// bool move(Fibre &&fibre, std::optional<int> priority)
/// {
///   // Capture the fibre handle. This allows the fibre argument to be invalidated while keeping
///   // a reference to the frame.
///   std::coroutine_handle<Fibre::promise_type> handle = fibre.__handle();
///   SharedQueue &queue = /* get target SharedQueue */;
///   // 1. Try pushing into the target queue (threadsafe).
///   // The SharedQueue performs the fibre.__release() on success.
///   const bool pushed = queue.tryPush(fibre);
///   if (pushed && priority)
///   {
///     // 2. Adjust priority on success, if necessary.
///     handle.promise().frame.priority = *priority;
///   }
///
///   // 3/4. Return success/failure.
///   return pushed;
/// }
/// @endcode
///
/// The potential to fail moving fibres allows the system to avoid potential deadlocks when the
/// target @c SharedQueue is full.
template <typename Scheduler>
concept SchedulerType =
  requires(Scheduler &scheduler, Fibre &fibre, std::optional<int32_t> priority) {
    { scheduler.move(fibre, priority) } -> std::same_as<bool>;
  };

/// Calculate the next power of two greater than or equal to the given @p value.
constexpr uint8_t nextPowerOfTwo(uint8_t value)
{
  if (value <= 1)
  {
    return 1;
  }

  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  return value + 1;
}

/// @overload
constexpr uint16_t nextPowerOfTwo(uint16_t value)
{
  if (value <= 1)
  {
    return 1;
  }

  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  return value + 1;
}

/// @overload
constexpr uint32_t nextPowerOfTwo(uint32_t value)
{
  if (value <= 1)
  {
    return 1;
  }

  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  return value + 1;
}

/// @overload
constexpr uint64_t nextPowerOfTwo(uint64_t value)
{
  if (value <= 1)
  {
    return 1;
  }

  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;
  value |= value >> 32;
  return value + 1;
}
}  // namespace morai
