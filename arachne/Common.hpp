#pragma once

#include <cstddef>
#include <functional>

namespace arachne
{
using IdValueType = uint64_t;
constexpr auto InvalidFibreValue = ~static_cast<IdValueType>(0);

struct Id
{
  IdValueType id = InvalidFibreValue;
};

constexpr Id InvalidFibre{ .id = InvalidFibreValue };
using WaitCondition = std::function<bool()>;

inline bool operator==(const Id &lhs, const Id &rhs) noexcept
{
  return lhs.id == rhs.id;
}

struct Time
{
  double epoch_time_s = 0.0;
  double dt = 0.0;
};

struct SchedulerParams
{
  uint32_t initial_queue_size = 1024u;
  std::vector<int32_t> priority_levels{};
};

inline uint8_t nextPowerOfTwo(uint8_t value)
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

inline uint16_t nextPowerOfTwo(uint16_t value)
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

inline uint32_t nextPowerOfTwo(uint32_t value)
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

inline uint64_t nextPowerOfTwo(uint64_t value)
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
}  // namespace arachne
