#pragma once

#include <functional>
#include <type_traits>

namespace morai
{
struct SchedulerParams
{
  uint32_t initial_queue_size = 1024u;
  uint32_t move_queue_size = 1024u;
  std::vector<int32_t> priority_levels{};
};

class Fibre;

template <typename Scheduler>
concept SchedulerType = requires(Scheduler &schduler, Fibre &&fibre) {
  { schduler.move(std::move(fibre)) } -> std::same_as<void>;
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
