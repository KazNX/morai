#pragma once

#include <cstddef>
#include <functional>

namespace arachne
{
using IdValueType = std::size_t;
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
}  // namespace arachne
