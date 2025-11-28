#pragma once

#include <morai/Clock.hpp>

namespace morai::test
{
[[nodiscard]] inline Clock makeClock(const double dt = 0.1)
{
  return Clock{ [dt, epoch_s = -dt]() mutable {
    epoch_s += dt;
    return epoch_s;
  } };
}
}  // namespace morai::test
