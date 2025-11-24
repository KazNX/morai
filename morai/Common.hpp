#pragma once

#include <cstdint>
#include <functional>
#include <optional>

namespace morai
{
struct SchedulerParams
{
  uint32_t initial_queue_size = 1024u;
  uint32_t move_queue_size = 1024u;
  std::vector<int32_t> priority_levels{};
};

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

struct Time
{
  double epoch_time_s = 0.0;
  double dt = 0.0;
};

using WaitCondition = std::function<bool()>;

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
