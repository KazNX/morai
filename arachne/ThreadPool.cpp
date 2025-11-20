#include "ThreadPool.hpp"

#include "Finally.hpp"
#include "Resumption.hpp"
#include "SharedQueue.hpp"

#include <algorithm>

namespace arachne
{
ThreadPool::ThreadPool(ThreadPoolParams params)
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
  SharedQueue &fibres = selectQueue(priority);
  fibres.push(std::move(fibre));
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

void ThreadPool::update(std::chrono::milliseconds time_slice)
{
  const auto target_end_time = std::chrono::steady_clock::now() + time_slice;
  while (std::chrono::steady_clock::now() < target_end_time)
  {
    if (!updateNextFibre())
    {
      break;
    }
  }
}

SharedQueue &ThreadPool::selectQueue(int32_t priority)
{
  SharedQueue *best = nullptr;

  for (auto &queue : _fibre_queues)
  {
    if (priority == queue->priority())
    {
      return *queue;
    }
    else if (priority > queue->priority())
    {
      // TODO: Log error that no exact priority match was made.
      return *queue;
    }
  }

  // Fallback to the lowest priority queue
  return *_fibre_queues.back();
}

void ThreadPool::pushFibre(Fibre &&fibre)
{
  SharedQueue &queue = selectQueue(fibre.priority());
  queue.push(std::move(fibre));
}

Fibre ThreadPool::nextFibre()
{
  for (auto &queue : _fibre_queues)
  {
    Fibre fibre = queue->pop();
    if (fibre.id() != InvalidFibre)
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

  std::ranges::sort(params.priority_levels);

  _fibre_queues.clear();
  for (const int32_t priority : params.priority_levels)
  {
    _fibre_queues.emplace_back(std::make_unique<SharedQueue>(priority, params.initial_queue_size));
  }
}

void ThreadPool::startWorkers(ThreadPoolParams &params)
{
  const int32_t thread_count =
    (params.thread_count > 0) ?
      params.thread_count :
      std::max(static_cast<int32_t>(std::thread::hardware_concurrency()) - params.thread_count, 1);

  for (int32_t thread_index = 0; thread_index < thread_count; ++thread_index)
  {
    _workers.emplace_back(&ThreadPool::workerThread, this, thread_index,
                          params.idle_sleep_duration);
  }
}

void ThreadPool::workerThread(int32_t thread_index, std::chrono::milliseconds idle_sleep_duration)
{
  while (!_quit.test())
  {
    if (_paused.test() || !updateNextFibre())
    {
      std::this_thread::sleep_for(idle_sleep_duration);
    }
  }
}

bool ThreadPool::updateNextFibre()
{
  // Get the next priority fibre.
  Fibre fibre = nextFibre();
  if (fibre.id() == InvalidFibre)
  {
    return false;
  }

  const double epoch_time_s = std::chrono::system_clock::now().time_since_epoch().count();
  const Resume resume = fibre.resume(epoch_time_s);
  if (resume.mode == ResumeMode::Expire) [[unlikely]]
  {
    // Expire the fibre.
    return true;
  }

  if (resume.mode == ResumeMode::Exception) [[unlikely]]
  {
    // Propagate exception and expire.
    std::exception_ptr ex = fibre.exception();
    // TODO: Log exception
    return true;  // Unreachable.
  }

  if (resume.reschedule) [[unlikely]]
  {
    const Priority reschedule = *resume.reschedule;
    const int32_t initial_priority = fibre.priority();
    if (initial_priority != reschedule.priority)
    {
      SharedQueue &new_queue = selectQueue(reschedule.priority);
      // Update fibre priority and reschedule.
      fibre.__setPriority(reschedule.priority);
    }
  }

  pushFibre(std::move(fibre));
  return true;
}
}  // namespace arachne
