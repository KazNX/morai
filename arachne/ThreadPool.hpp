#pragma once

#include "Common.hpp"
#include "Fibre.hpp"
#include "SharedQueue.hpp"

#include <chrono>
#include <thread>
#include <string_view>

namespace arachne
{
struct ThreadPoolParams : public SchedulerParams
{
  /// Number of threads to use in the thread pool. Special semantics are given
  /// to zero and negative values:
  ///
  /// - 0: Use all available threads - @c std::thread::hardware_concurrency()
  /// - -1: Use available threads minus one.
  /// - -N: Use available threads minus N.
  int32_t thread_count = 0u;
  std::chrono::milliseconds idle_sleep_duration{ 1 };
};

/// A multi-threaded task scheduler using fibres (coroutines) as tasks.
class ThreadPool
{
public:
  ThreadPool(ThreadPoolParams params = {});
  ~ThreadPool();

  /// Returns true if there are no running fibres.
  [[nodiscard]] bool empty() const noexcept;

  /// Returns the (approximate) number of running fibres regardless of suspended state.
  [[nodiscard]] std::size_t runningCount() const noexcept;

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

  // /// Generate a wait condition to wait until the specified fibre is no longer
  // /// running.
  // /// @param fibre_id The fibre ID to wait for.
  // /// @return The wait condition. Can be used with @c co_await in a fibre.
  // WaitCondition await(const Id fibre_id) const noexcept
  // {
  //   return [this, fibre_id]() -> bool { return !isRunning(fibre_id); };
  // }

  /// Cancel all running fibres.
  void cancelAll();

  /// Have this (main) thread join in the scheduling for at least the given time slice.
  void update(std::chrono::milliseconds time_slice);

  bool wait(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

private:
  SharedQueue &selectQueue(int32_t priority);
  void pushFibre(Fibre &&fibre);
  [[nodiscard]] Fibre nextPriorityFibre();
  [[nodiscard]] Fibre nextFibre();

  void createQueues(ThreadPoolParams &params);
  void startWorkers(ThreadPoolParams &params);
  void workerThread(int32_t thread_index, std::chrono::milliseconds idle_sleep_duration);
  bool updateNextFibre();

  std::vector<std::unique_ptr<SharedQueue>> _fibre_queues;
  std::vector<std::jthread> _workers;
  std::atomic_flag _paused = ATOMIC_FLAG_INIT;
  std::atomic_flag _quit = ATOMIC_FLAG_INIT;
  std::chrono::milliseconds _idle_sleep_duration{ 1 };
};
}  // namespace arachne
