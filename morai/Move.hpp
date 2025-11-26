#pragma once

#include "Common.hpp"

namespace morai
{
/// Helper object returned by @c moveTo() in order to map to the correct @c  await_transform().
/// This is an implementation detail - use @c moveTo() functions.
template <typename Scheduler>
  requires SchedulerType<Scheduler>
struct MoveTo
{
  Scheduler *target = nullptr;        ///< Pointer to the @c Scheduler to move to.
  std::optional<int32_t> priority{};  ///< Optional priority to schedule at.
};

/// Used with @c co_await in fibres to move the fibre to another @c Scheduler.
///
/// See @c Fibre @c co_await handling.
///
/// @tparam Scheduler The target @c Scheduler type - see @c SchedulerType concept.
/// @param scheduler The target @c Scheduler to move the fibre to.
/// @param priority Optional priority level to schedule the fibre at on the target @c Scheduler.
/// Preserves the current @c Fibre priority if not specified.
/// @return A @c MoveTo object used to invoke the correct @c Fibre::promise_type::await_transform().
template <typename Scheduler>
  requires SchedulerType<Scheduler>
[[nodiscard]] MoveTo<Scheduler> moveTo(Scheduler &scheduler,
                                       std::optional<int32_t> priority = std::nullopt)
{
  return { .target = &scheduler, .priority = priority };
}

/// @overload
template <typename Scheduler>
  requires SchedulerType<Scheduler>
[[nodiscard]] MoveTo<Scheduler> moveTo(Scheduler *scheduler,
                                       std::optional<int32_t> priority = std::nullopt)
{
  return { .target = scheduler, .priority = priority };
}
}  // namespace morai
