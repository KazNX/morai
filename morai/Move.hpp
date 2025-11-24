#pragma once

#include "Common.hpp"

namespace morai
{
class Fibre;

template <typename Scheduler>
  requires SchedulerType<Scheduler>
struct MoveTo
{
  Scheduler *target = nullptr;
  std::optional<int32_t> priority{};
};

template <typename Scheduler>
  requires SchedulerType<Scheduler>
MoveTo<Scheduler> moveTo(Scheduler &scheduler, std::optional<int32_t> priority = std::nullopt)
{
  return { .target = &scheduler, .priority = priority };
}

template <typename Scheduler>
  requires SchedulerType<Scheduler>
MoveTo<Scheduler> moveTo(Scheduler *scheduler, std::optional<int32_t> priority = std::nullopt)
{
  return { .target = scheduler, .priority = priority };
}
}  // namespace morai
