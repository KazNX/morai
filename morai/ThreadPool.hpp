#pragma once

#include "Common.hpp"
#include "Fibre.hpp"
#include "SharedQueue.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <thread>
#include <string_view>

namespace morai
{
struct ThreadPoolParams : public SchedulerParams
{
  /// Number of threads to use in the thread pool. Special semantics are given
  /// to the following value ranges:
  ///
  /// - No value - use all available threads: @c std::thread::hardware_concurrency()
  /// - Positive N: Use N threads, up to @c std::thread::hardware_concurrency()
  /// - 0: Create zero workers. User must call @c ThreadPool::update().
  /// - -1: Use available threads minus one.
  /// - -N: Use available threads minus N (at least 1).
  std::optional<int32_t> worker_count = 0u;
  std::chrono::milliseconds idle_sleep_duration{ 1 };
};

/// A multi-threaded task scheduler using fibres (coroutines) as tasks.
///
/// The thread pool is created with a number of worker threads - see
/// @c ThreadPoolParams::worker_count - which continually pop tasks then return them to the worker
/// queues.
///
/// The @c ThreadPool also supports priority scheduling. Workers prefer draining higher priority
/// (lower value) queues.
///
/// @c ThreadPool::worker_count may be zero in which case the user must call @c update() must be
/// called to process tasks. This can be used to control the thread pool manually.
/// Unlike the @c Scheduler, the @c ThreadPool has fixed size queues. Calling @c start() blocks
/// until there is space in the queue to add the new fibre to the target priority queue. As such
/// it is possible to deadlock the thread pool as all workers try to push back their fibre to a full
/// queue. There is current no solution to this issue. This process also sleeps the pushing thread
/// for @c ThreadPoolParams::idle_sleep_duration so pushing to a full queue is expensive.
class ThreadPool
{
public:
  ThreadPool(ThreadPoolParams params = {});
  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;

  /// Returns true if there are no running fibres.
  [[nodiscard]] bool empty() const noexcept;

  /// Returns the (approximate) number of running fibres regardless of suspended state.
  [[nodiscard]] std::size_t runningCount() const noexcept;

  /// Return the number of worker threads in this pool. Could be zero in which case @c update() must
  /// be called manually to process tasks.
  [[nodiscard]] std::size_t workerCount() const noexcept { return _workers.size(); }

  /// Start a fibre.
  ///
  /// This fibre is added to the scheduler and assigned the returned @c Id. The fibre entry point is
  /// a coroutine using at least one of @c co_await, @c co_yield or @c co_return and with a
  /// @c fibre::Fibre return type.
  ///
  /// @code
  /// morai::ThreadPool pool;
  /// morai::Id fibre_id = pool.start([]() -> morai::Fibre {
  ///   std::cout << "Fibre started\n";
  ///   co_await 1.0; // Sleep for one second
  ///   std::cout << "Fibre done\n";
  /// }()); // Note '()' as the lambda is immediately invoked.
  /// @endcode
  ///
  /// @param fibre The fibre entry point.
  /// @param priority Scheduling priority.
  /// @param name Optional name.
  /// @return The fibre @c Id. Maybe used for cancellation or @c await().
  Id start(Fibre &&fibre, int32_t priority = 0, std::string_view name = {});
  /// @overload
  Id start(Fibre &&fibre, std::string_view name)
  {
    return start(std::move(fibre), 0, std::move(name));
  }

  /// Cancel all running fibres.
  void cancelAll();

  /// Have this thread join in the scheduling consuming tasks until there are none available or
  /// @c continue_condition returns false.
  ///
  /// Threadsafe so long as the @p continue_condition is threadsafe.
  void update(std::function<bool()> continue_condition);

  /// Overload of @c update() which sets the condition to false once the specified time slice
  /// has elapsed.
  ///
  /// Threadsafe.
  void update(std::chrono::milliseconds time_slice);

  /// Wait for all tasks to complete within the specified timeout.
  ///
  /// Note this stops waiting if at any point the #queues are empty. However, this can happen
  /// because all tasks have been moved to worker threads so there's no guarantee that all tasks are
  /// complete. Additionally, the return value comes after the loop condition is checked, so it's
  /// also possible to return false before the timeout elapses.
  ///
  /// @param timeout The maximum time to wait, or indefinite wait on @c std::nullopt.
  /// @return True of the queues are empty (unreliable).
  bool wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

  /// Move a fibre into the task pool (theadsafe). This implements the scheduler move operations.
  ///
  /// On success the @p fibre coroutine handle is moved out of the @p fibre object into a new
  /// @c Fibre object, invalidating the @p fibre argument. On failure @p fibre remains valid.
  /// Success is indicated by the return value.
  ///
  /// Fibres are immediately inserted into the priority queue most closely matching the fibre
  /// priority (lower bound).
  ///
  /// @param fibre A reference to the fibre to move.
  /// @return True on success, in which case the @p fibre argument becomes invalid.
  bool move(Fibre &fibre, std::optional<int32_t> priority = std::nullopt);

private:
  SharedQueue &selectQueue(int32_t priority, bool quiet);
  [[nodiscard]] bool tryPushFibre(Fibre &fibre);
  [[nodiscard]] Fibre nextPriorityFibre();
  [[nodiscard]] Fibre nextFibre(uint32_t &selection_index);

  void createQueues(ThreadPoolParams &params);
  void startWorkers(ThreadPoolParams &params);
  void workerThread(int32_t thread_index, std::chrono::milliseconds idle_sleep_duration);
  bool updateNextFibre(uint32_t &selection_index);

  std::vector<std::unique_ptr<SharedQueue>> _fibre_queues;
  std::vector<uint32_t> _queue_weighted_selection;
  std::vector<std::jthread> _workers;
  std::atomic_flag _paused = ATOMIC_FLAG_INIT;
  std::atomic_flag _quit = ATOMIC_FLAG_INIT;
  std::chrono::milliseconds _idle_sleep_duration{ 1 };
};
}  // namespace morai
