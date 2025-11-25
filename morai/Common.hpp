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
/// - `Fibre move(Fibre &&fibre, std::optional<int> priority);`
///
/// There are some odd constraints here. The @c Fibre is moved to the target function. If the move
/// operation can succeed, then the @c move() function should return an empty/invalid fibre:
/// @c Fibre{} . On failure, the function should return the incoming fibre as shown below.
///
/// @code
/// Fibre move(Fibre &&fibre, std::optional<int> priority)
/// {
///   SharedQueue &queue = /* get target SharedQueue */;
///   return queue.push(std::move(fibre));
/// }
/// @endcode
///
/// On success, the fibre is essentially moved to the target queue. On failure the fibre is
/// temporarily move to the queue, then returned by the queue and moved out of the function.
///
/// Another example below, shows a move attempt that always fails:
///
/// @code
/// Fibre move(Fibre &&fibre, std::optional<int> priority)
/// {
///   // Always move the fibre back out.
///   return std::move(fibre);
/// }
/// @endcode
///
/// This odd ownership movement allows calling code to deal with potential deadlocks where the
/// target @c SharedQueue is full.
template <typename Scheduler>
concept SchedulerType =
  requires(Scheduler &scheduler, Fibre &&fibre, std::optional<int32_t> priority) {
    { scheduler.move(std::move(fibre), priority) } -> std::same_as<Fibre>;
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
