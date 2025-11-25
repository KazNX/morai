#pragma once

#include "Common.hpp"

#include <cstdint>
#include <chrono>

namespace morai
{
/// Rescheduling ordering preference.
enum class PriorityPosition : int8_t
{
  Front,  ///< Prefer inserting at the start of the new priority level.
  Back    ///< Prefer inserting at the back of the new priority level.
};

/// Priority object, used for rescheduling.
struct Priority
{
  int32_t priority = 0;                                ///< New priority level.
  PriorityPosition position = PriorityPosition::Back;  ///< Ordering preference.
};

/// Return values for @c Fibre::resume(), indicating the new state of the fibre.
enum class ResumeMode : std::int8_t
{
  Continue,   ///< Fibre continued running come code - push back into the queue, may need
              ///< rescheduling.
  Sleep,      ///< Fibre is sleeping or waiting - push back into the queue.
  Moved,      ///< Moved to another scheduler - do nothing more in the current scheduler.
  Expire,     ///< Fibre has expired and requires cleanup - do nothing more.
  Exception,  ///< An exception was raised. Propagate or log the exception - do not reschedule.
};

/// Return value for @c Fibre::resume(), indicating what to do next with the fibre.
struct Resume
{
  /// How to resume.
  ResumeMode mode = ResumeMode::Continue;
  /// Rescheduling information for @c ResumeMode::Continue.
  std::optional<Priority> reschedule = {};
};

/// This object tells the scheduler how or when to resume a fibre.
///
/// This object may be used with @c co_yield or @c co_await statements, however
/// the preferred practice is to use @c yield(), @c sleep(), @c wait() functions
/// or to use other supported @c co_await expressions - see @c Scheduler.
struct Resumption
{
  /// Resumption time value. When set, this is specified as a relative time value. Internally this
  /// value is converted into an absolute epoch time.
  double time_s = 0;
  /// Optional condition to wait on before resuming.
  WaitCondition condition = {};
};

inline Priority reschedule(int32_t priority, PriorityPosition position = PriorityPosition::Back)
{
  return { .priority = priority, .position = position };
}

/// A helper function for unqualified yield.
///
/// Example usage:
///
/// @code
/// using namespace morai;
///
/// Fibre fibre_entrypoint()
/// {
///   co_yield yield(); // Basic usage.
///   co_yield {};      // Equivalent to the above - preferred.
///   co_await yield(); // Equivalent to above.
/// }
/// @endcode
inline Resumption yield()
{
  return Resumption{};
}

/// A helper function for specifying a sleep duration.
///
/// Note that there is no guarantee that a fibre will resume after exactly the specified duration.
/// The granularity of the @c Scheduler::update() controls the minimum sleep duration.
///
/// Example usage:
///
/// @code
/// using namespace morai;
///
/// Fibre fibre_entrypoint()
/// {
///   co_await sleep(1.0);  // Preferred usage.
///   co_yield sleep(1.0);  // Supported alternative usage.
/// }
/// @endcode
///
/// @param duration_s The sleep duration in seconds.
inline Resumption sleep(const double duration_s)
{
  return Resumption{ .time_s = duration_s };
}

/// A helper function for specifying a sleep duration using a chrono duration.
///
/// Example usage:
///
/// @code
/// using namespace morai;
///
/// Fibre fibre_entrypoint()
/// {
///   co_await sleep(std::chrono::millisconds(1000));
///   using namespace std::chrono_literals;
///   co_await sleep(1000ms);
///   co_yield sleep(1000ms); // Supported alternative usage.
/// }
/// @endcode
///
/// @param duration The sleep duration.
template <typename Rep, typename Period>
Resumption sleep(const typename std::chrono::duration<Rep, Period> &duration)
{
  return { .time_s = std::chrono::duration<double>(duration).count() };
}

/// A helper function for specifying a wait condition with optional timeout.
///
/// The fibre will resume after @c condition returns true, or after the timeout has expired (if
/// set. Note that the fibre may resume with @c condition reporting @c false when a timeout is
/// specified. There is no other way to check if the timeout expired or the condition was met.
///
/// Example usage:
///
/// @code
/// using namespace morai;
///
/// Fibre fibre_entrypoint(bool* condition_met)
/// {
///   // Preferred usage:
///   co_await wait([condition_met]() { return *condition_met; }, 15.0);
///   // Alternative, supported usage:
///   co_yield wait([condition_met]() { return *condition_met; }, 15.0);
/// }
/// @endcode
///
/// @param duration The sleep duration.
inline Resumption wait(WaitCondition condition, const double timeout_s = 0)
{
  return Resumption{ .time_s = timeout_s, .condition = std::move(condition) };
}

/// @overload
template <typename Rep, typename Period>
Resumption wait(WaitCondition condition,
                const typename std::chrono::duration<Rep, Period> &timeout_duration)
{
  return Resumption{ .time_s = std::chrono::duration<double>(timeout_duration).count(),
                     .condition = std::move(condition) };
}
}  // namespace morai
