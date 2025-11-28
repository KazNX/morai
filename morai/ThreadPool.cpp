#include "ThreadPool.hpp"

#include "Finally.hpp"
#include "Log.hpp"
#include "Resumption.hpp"
#include "SharedQueue.hpp"

#include <algorithm>
#include <thread>

namespace morai
{
namespace
{
void generateQueueSelectionSet(std::vector<uint32_t> &set, uint32_t queue_count)
{
  // Simple weighting: given 4 queues, we generate:
  //  { 0, 0, 0, 0, 1, 1, 1, 2, 2, 3 }
  // - Update [0] 4 times
  // - Update [1] 3 times
  // - Update [2] 2 times
  // - Update [3] 1 times
  for (uint32_t i = 0; i < queue_count; ++i)
  {
    const auto weighting = (queue_count - i);
    for (uint32_t j = 0; j < weighting; ++j)
    {
      set.emplace_back(i);
    }
  }
}
}  // namespace

ThreadPool::ThreadPool(ThreadPoolParams params)
  : ThreadPool{ Clock{}, std::move(params) }
{}

ThreadPool::ThreadPool(Clock clock, ThreadPoolParams params)
  : _idle_sleep_duration(params.idle_sleep_duration)
  , _clock(std::move(clock))
{
  createQueues(params);
  startWorkers(params);
}

ThreadPool::~ThreadPool()
{
  _quit.test_and_set();
  cancelAll();
  for (auto &thread : _workers)
  {
    if (thread.joinable())
    {
      thread.join();
    }
  }
}

bool ThreadPool::empty() const noexcept
{
  for (const auto &queue : _fibre_queues)
  {
    if (!queue->empty())
    {
      return false;
    }
  }

  return true;
}

std::size_t ThreadPool::runningCount() const noexcept
{
  std::size_t count = 0;
  for (const auto &queue : _fibre_queues)
  {
    count += queue->size();
  }
  return count;
}

Id ThreadPool::start(Fibre &&fibre, int32_t priority, std::string_view name)
{
  // Fibre creation assigned the ID. We need to store it before moving the fibre.
  Id fibre_id = fibre.id();
  fibre.__setPriority(priority);
  fibre.setName(name);
  SharedQueue &fibres = selectQueue(priority, false);
  while (!fibres.tryPush(fibre))
  {
    // Full. Sleep and try again.
    std::this_thread::sleep_for(_idle_sleep_duration);
  }
  return fibre_id;
}

void ThreadPool::cancelAll()
{
  const auto resume = finally([this]() { _paused.clear(); });
  _paused.test_and_set();
  for (auto &queue : _fibre_queues)
  {
    queue->clear();
  }
}

void ThreadPool::update(std::function<bool()> continue_condition)
{
  uint32_t selection_index = 0;
  while (continue_condition())
  {
    if (!updateNextFibre(selection_index))
    {
      break;
    }
  }
}

void ThreadPool::update(std::chrono::milliseconds time_slice)
{
  update([target_end_time = std::chrono::steady_clock::now() + time_slice]() {
    return std::chrono::steady_clock::now() < target_end_time;
  });
}

bool ThreadPool::wait(std::optional<std::chrono::milliseconds> timeout)
{
  const auto target_end_time = timeout.has_value() ? std::chrono::steady_clock::now() + *timeout :
                                                     std::chrono::steady_clock::time_point::max();

  while (!empty() && std::chrono::steady_clock::now() < target_end_time)
  {
    std::this_thread::sleep_for(_idle_sleep_duration);
  }

  return empty();
}

bool ThreadPool::move(Fibre &fibre, std::optional<int32_t> priority)
{
  // Grab the so that we only adjust the priority on success. The Fibre object will be invalid by
  // then.
  std::coroutine_handle<Fibre::promise_type> handle = fibre.__handle();
  // Unlike scheduler, we can directly insert into the target queue as they are all threadsafe.
  SharedQueue &queue = selectQueue(*priority, false);
  const bool pushed = queue.tryPush(fibre);
  if (pushed && priority)
  {
    handle.promise().frame.priority = *priority;
  }
  return pushed;
}

SharedQueue &ThreadPool::selectQueue(int32_t priority, bool quiet)
{
  size_t best_idx = 0;

  for (size_t i = 0; i < _fibre_queues.size(); ++i)
  {
    SharedQueue &queue = *_fibre_queues.at(i);
    if (priority == queue.priority())
    {
      return queue;
    }
    else if (priority > queue.priority())
    {
      best_idx = i;
    }
    else if (priority < queue.priority())
    {
      break;
    }
  }

  SharedQueue &queue = *_fibre_queues.at(best_idx);
  log::error(std::format("Thread Pool: Fibre priority mismatch: {} moved to {}", priority,
                         queue.priority()));

  return queue;
}

bool ThreadPool::tryPushFibre(Fibre &fibre)
{
  SharedQueue &queue = selectQueue(fibre.priority(), true);
  return queue.tryPush(fibre);
}

Fibre ThreadPool::nextFibre(uint32_t &selection_index)
{
  // FIXME: this will result in low priority starvation.
  for (size_t i = 0; i < _queue_weighted_selection.size(); ++i)
  {
    SharedQueue &queue = *_fibre_queues.at(_queue_weighted_selection.at(selection_index));
    selection_index = selection_index =
      (selection_index + 1u) % static_cast<uint32_t>(_queue_weighted_selection.size());
    Fibre fibre = queue.pop();
    if (fibre.valid())
    {
      return fibre;
    }
  }

  return Fibre{};
}

void ThreadPool::createQueues(ThreadPoolParams &params)
{
  if (params.priority_levels.empty())
  {
    params.priority_levels.push_back(0);
  }

  generateQueueSelectionSet(_queue_weighted_selection, params.priority_levels.size());

  std::ranges::sort(params.priority_levels);

  _fibre_queues.clear();
  for (const int32_t priority : params.priority_levels)
  {
    _fibre_queues.emplace_back(std::make_unique<SharedQueue>(priority, params.initial_queue_size));
  }
}

void ThreadPool::startWorkers(ThreadPoolParams &params)
{
  int32_t worker_count = std::thread::hardware_concurrency();

  if (params.worker_count.has_value())
  {
    worker_count = *params.worker_count;
    if (worker_count < 0)
    {
      worker_count =
        std::max(static_cast<int32_t>(std::thread::hardware_concurrency()) + worker_count, 1);
    }
  }

  _workers.reserve(worker_count);
  for (int32_t thread_index = 0; thread_index < worker_count; ++thread_index)
  {
    _workers.emplace_back(&ThreadPool::workerThread, this, thread_index,
                          params.idle_sleep_duration);
  }
}

void ThreadPool::workerThread(int32_t thread_index, std::chrono::milliseconds idle_sleep_duration)
{
  uint32_t selection_index = 0;
  while (!_quit.test())
  {
    if (_paused.test() || !updateNextFibre(selection_index))
    {
      std::this_thread::sleep_for(idle_sleep_duration);
    }
  }
}

bool ThreadPool::updateNextFibre(uint32_t &selection_index)
{
  // Get the next priority fibre.
  Fibre fibre = nextFibre(selection_index);
  while (fibre.valid())
  {
    const double epoch_time_s = _clock.epoch();
    const Resume resume = fibre.resume(epoch_time_s);
    if (resume.mode == ResumeMode::Expire || resume.mode == ResumeMode::Moved) [[unlikely]]
    {
      // Expire the fibre.
      return true;
    }

    if (resume.mode == ResumeMode::Exception) [[unlikely]]
    {
      // Propagate exception and expire.
      std::exception_ptr ex = fibre.exception();
      try
      {
        if (ex)
        {
          std::rethrow_exception(ex);
        }
      }
      catch (const std::exception &e)
      {
        log::error(std::format("Thread pool fibre {}:{} exception: {}", fibre.id().id(),
                               fibre.name(), e.what()));
      }
      catch (...)
      {
        log::error(
          std::format("Thread pool fibre {}:{} unknown exception.", fibre.id().id(), fibre.name()));
      }
      return true;  // Unreachable.
    }

    if (resume.reschedule) [[unlikely]]
    {
      const Priority reschedule = *resume.reschedule;
      const int32_t initial_priority = fibre.priority();
      if (initial_priority != reschedule.priority)
      {
        SharedQueue &new_queue = selectQueue(reschedule.priority, true);
        // Update fibre priority and reschedule.
        fibre.__setPriority(reschedule.priority);
      }
    }

    // Try requeue the fibre. This may fail if the queue is full. In this case we'll update the
    // fibre again, hoping the queues will free up. While this avoids a total deadlock, it can still
    // result in fibre starvation.
    if (tryPushFibre(fibre))
    {
      return true;
    }
  }
  return false;
}
}  // namespace morai
