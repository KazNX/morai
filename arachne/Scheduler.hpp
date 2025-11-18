#pragma once

#include "Common.hpp"
#include "Fibre.hpp"
#include "FibreQueue.hpp"

#include <array>
#include <span>
#include <string_view>

namespace arachne
{
struct Time
{
  double epoch_time_s = 0.0;
  double dt = 0.0;
};

/// Implements a single threaded fibre scheduler.
///
/// The fibre @c Scheduler implements cooperative multitasking in a single thread. Fibres are
/// written as C++20 coroutines. There are two requirements for implementing a fibre entrypoint:
///
/// - The entry point function must have a @c fibre::Fibre return type.
/// - The entry point function must be a C++ coroutine - i.e., use @c co_await, @c co_yield or
///   @c co_return.
///
/// The scheduler supports starting fibres via @c start() and requires @c update() to be called
/// in order to update all known fibres. The provided epoch time must be monotonically increasing,
/// but is otherwise managed by the caller. The time is interpreted as seconds, though the actual
/// rate may be varied by the called.
///
/// All fibres start initially suspended and remain suspended until the next @c update() call.
/// Fibres *must* cede control regularly or starve the @c fibre::Scheduler thread as there is no
/// pre-emption.
///
/// A fibre may cede control in any for the following ways (preferred usage is marked):
///
/// - `co_yield {};` - resume next update. (preferred)
///   - `co_yield fibre::yield();` - resume next update (alias)
///   - `co_await fibre::yield();` - supported alternative
/// - `co_await <double>;` - resume after the specified number of seconds. (preferred)
///   - `co_await std::chrono::duration...;` (alternative preferred)
///   - `co_await fibre::sleep(duration_s);` - resume after duration_s seconds (alias)
///   - `co_yield <sleep-expression>;` - supported alternative
/// - `co_await fibre::wait(condition[, timeout]);` - resume when condition() returns true
///   (preferred)
///   - `co_await []() -> bool { ... };` - resume when the lambda returns true (alternative
///     preferred)
///   - `co_yield fibre::wait(condition[, timeout]);` - supported alternative
/// - `co_return;` - end fibre execution. Implicit on reaching the end of the function.'
///
/// All @c co_await expressions will continue immediately if the condition is already met (not
/// possible when awaiting a @c fibre::yield() ). Conversely, all @c co_yield expressions cede
/// control even if the condition is already met.
///
/// Typical usage of the scheduler is as follows:
///
/// @code
/// int main()
/// {
///   using namespace arachne;
///   Scheduler fibres;     // Create the scheduler
///   startFibres(fibres);  // Start some fibres
///   while (!fibres.empty())
///   {
///     const double epoch_time_s = getEpochTime(); // Get current time
///     fibres.update(epoch_time_s);                // Update all fibres
///     doOtherWork();                              // Do other work
///   }
///   return 0;
/// }
/// @endcode
///
/// Other notes:
///
/// - A @c fibre::Scheduler is not itself thread-safe.
/// - Multiple fibre systems may exist both on the same thread or across multiple threads.
/// - Fibres may not be moved between fibre systems.
/// - @c gls::finally() patterns may be used to ensure some fibre cleanup code is always executed.
/// - Fibres may start other fibres in the same @c Scheduler so long as they have access to the
///   @c Scheduler. New fibres are updated on the *next* @c update() call.
/// - Fibres may cancel other fibres in the same @c Scheduler. This prevents any further updates of
/// the
///   target fibre.
class Scheduler
{
public:
  Scheduler();
  ~Scheduler();

  /// Returns true if there are no running fibres.
  [[nodiscard]] bool empty() const noexcept { return runningCount() == 0; }
  /// Returns the number of running fibres regardless of suspended state.
  [[nodiscard]] std::size_t runningCount() const noexcept
  {
    return _fibre_queues[0].size() + _fibre_queues[1].size();
  }
  /// Check if there is a fibre running with the given ID.
  [[nodiscard]] bool isRunning(Id fibre_id) const noexcept;

  [[nodiscard]] const Time &time() const noexcept { return _time; }

  /// Start a fibre.
  ///
  /// This fibre is added to the scheduler and assigned the returned @c Id. The fibre entry point is
  /// a coroutine using at least one of @c co_await, @c co_yield or @c co_return and with a
  /// @c fibre::Fibre return type.
  ///
  /// @code
  /// arachne::Scheduler scheduler;
  /// arachne::Id fibre_id = scheduler.start([]() -> arachne::Fibre {
  ///   std::cout << "Fibre started\n";
  ///   co_await 1.0; // Sleep for one second
  ///   std::cout << "Fibre done\n";
  /// }()); // Note '()' as the lambda is immediately invoked.
  /// @endcode
  ///
  /// @param fibre The fibre entry point.
  /// @return The fibre @c Id. Maybe used for cancellation or @c await().
  Id start(Fibre &&fibre, int32_t priority = 0, std::string_view name = {});
  Id start(Fibre &&fibre, std::string_view name)
  {
    return start(std::move(fibre), 0, std::move(name));
  }

  /// Cancel a running fibre by @c Id.
  /// @param fibre_id @c Id of the fibre to cancel.
  /// @return True if a fibre matching the @p fibre_id was found an cancelled.
  bool cancel(Id fibre_id);
  /// Cancel multiple running fibres by @c Id.
  std::size_t cancel(std::span<const Id> fibre_ids);

  /// Generate a wait condition to wait until the specified fibre is no longer
  /// running.
  /// @param fibre_id The fibre ID to wait for.
  /// @return The wait condition. Can be used with @c co_await in a fibre.
  WaitCondition await(const Id fibre_id) const noexcept
  {
    return [this, fibre_id]() -> bool { return !isRunning(fibre_id); };
  }

  /// Cancel all running fibres.
  void cancelAll();

  /// Update all fibres. Blocks until all fibres have been updated. A blocking
  /// fibre can stall the scheduler.
  /// @param epoch_time_s The current epoch time in seconds. This is user
  /// defined, but must be monotonically increasing.
  /// @c std::chrono::system_clock::now().time_since_epoch() makes for a
  /// reasonable default.
  void update(double epoch_time_s);

private:
  FibreQueue &activeQueue() noexcept { return _fibre_queues[_active_queue]; }
  FibreQueue &inactiveQueue() noexcept { return _fibre_queues[1 - _active_queue]; }
  void swapQueues() noexcept { _active_queue = 1 - _active_queue; }

  std::array<FibreQueue, 2> _fibre_queues{ FibreQueue{}, FibreQueue{} };
  int32_t _active_queue = 0;
  /// Fibres added during an update. Migrated at the end of the update.
  IdValueType _next_id = 0u;
  Time _time{};
};
}  // namespace arachne
