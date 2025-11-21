#pragma once

#include "Common.hpp"

namespace arachne
{
class Fibre;

template <typename Scheduler>
  requires SchedulerType<Scheduler>
Scheduler *moveTo(Scheduler &scheduler)
{
  return &scheduler;
}

template <typename Scheduler>
  requires SchedulerType<Scheduler>
Scheduler *moveTo(Scheduler *scheduler)
{
  return { .target = scheduler };
}
}  // namespace arachne
