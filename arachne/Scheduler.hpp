#pragma once

#include "Common.hpp"
#include "FibreQueue.hpp"
#include "SharedQueue.hpp"

#include <span>
#include <string_view>

namespace arachne
{
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
  Scheduler(SchedulerParams params = {});
  ~Scheduler();

  Scheduler(const Scheduler &) = delete;
  Scheduler(Scheduler &&) = delete;
  Scheduler &operator=(const Scheduler &) = delete;
  Scheduler &operator=(Scheduler &&) = delete;

  /// Returns true if there are no running fibres.
  [[nodiscard]] bool empty() const noexcept { return runningCount() == 0; }
  /// Returns the number of running fibres regardless of suspended state.
  [[nodiscard]] std::size_t runningCount() const noexcept
  {
    std::size_t count = 0;
    for (const auto &queue : _fibre_queues)
    {
      count += queue.size();
    }
    return count + _move_queue.size();
  }

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
  /// @return The fibre @c Id. Maybe used for cancellation or @c co_await.
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

  /// Cancel all running fibres.
  void cancelAll();

  /// Update all fibres. Blocks until all fibres have been updated. A blocking
  /// fibre can stall the scheduler.
  /// @param epoch_time_s The current epoch time in seconds. This is user
  /// defined, but must be monotonically increasing.
  /// @c std::chrono::system_clock::now().time_since_epoch() makes for a
  /// reasonable default.
  void update(double epoch_time_s);

  /// Move a fibre into this scheduler (threadsafe).
  void move(Fibre &&fibre);

private:
  Id enqueue(Fibre &&fibre);

  FibreQueue &selectQueue(int32_t priority, bool quiet);
  void updateQueue(double epoch_time_s, FibreQueue &queue);

  void pumpMoveQueue();

  std::vector<FibreQueue> _fibre_queues;
  SharedQueue _move_queue;
  Time _time{};
};
}  // namespace arachne
