#include "Scheduler.hpp"

#include "Log.hpp"

#include <algorithm>
#include <coroutine>

namespace morai
{
Scheduler::Scheduler(SchedulerParams params)
  : _move_queue(0, params.move_queue_size)
{
  std::ranges::sort(params.priority_levels);
  // Ensure at least one queue.
  if (params.priority_levels.empty())
  {
    params.priority_levels.emplace_back(0);
  }

  for (const int32_t priority_level : params.priority_levels)
  {
    _fibre_queues.emplace_back(priority_level, params.initial_queue_size);
  }
}

Scheduler::~Scheduler() = default;

Id Scheduler::start(Fibre &&fibre, int32_t priority, std::string_view name)
{
  // Fibre creation assigned the ID. We need to store it before moving the fibre.
  Id fibre_id = fibre.id();
  fibre.__setPriority(priority);
  fibre.setName(name);
  return enqueue(std::move(fibre));
}

bool Scheduler::cancel(const Id fibre_id)
{
  if (!fibre_id.valid())
  {
    return false;
  }

  for (auto &queue : _fibre_queues)
  {
    if (queue.cancel(fibre_id))
    {
      return true;
    }
  }
  return false;
}

std::size_t Scheduler::cancel(std::span<const Id> fibre_ids)
{
  std::size_t removed = 0;
  for (const Id &id : fibre_ids)
  {
    removed += !!cancel(id);
  }
  return removed;
}

void Scheduler::cancelAll()
{
  for (auto &queue : _fibre_queues)
  {
    queue.clear();
  }
  _move_queue.clear();
}

void Scheduler::update(const double epoch_time_s)
{
  _time.dt = epoch_time_s - _time.epoch_time_s;
  _time.epoch_time_s = epoch_time_s;


  for (auto &fibre_queue : _fibre_queues)
  {
    updateQueue(epoch_time_s, fibre_queue);
  }
}

Fibre Scheduler::move(Fibre &&fibre, std::optional<int32_t> priority)
{
  // Grab the so that we only adjust the priority on success. The Fibre object will be invalid by
  // then.
  std::coroutine_handle<Fibre::promise_type> handle = fibre.__handle();
  Fibre residual = _move_queue.push(std::move(fibre));
  if (priority && !residual.valid())
  {
    handle.promise().frame.priority = *priority;
  }
  return residual;
}

Id Scheduler::enqueue(Fibre &&fibre)
{
  FibreQueue &fibres = selectQueue(fibre.priority(), false);
  Id id = fibre.id();  // Cache Id before move.
  fibres.push(std::move(fibre));
  return id;
}

FibreQueue &Scheduler::selectQueue(int32_t priority, bool quiet)
{
  size_t best_idx = 0;

  for (size_t i = 0; i < _fibre_queues.size(); ++i)
  {
    FibreQueue &queue = _fibre_queues.at(i);
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

  FibreQueue &queue = _fibre_queues.at(best_idx);
  log::error(
    std::format("Scheduler: Fibre priority mismatch: {} moved to {}", priority, queue.priority()));

  return queue;
}

void Scheduler::updateQueue(const double epoch_time_s, FibreQueue &queue)
{
  // Move a pending item from the move queue.
  pumpMoveQueue();

  // Update N times where N is the size. Note the size may change during iteration as new fibres
  // are added, or fibres removed. Expired fibres are not reinserted, so the size would shrink. We
  // account for this by tracking expired_count. New fibres may cause unbounded growth.
  size_t expired_count = 0;
  for (size_t i = 0; i < queue.size() + expired_count; ++i)
  {
    // On every iteration, we pump the move queue. It has limited capacity and hitting that capacity
    // can cause deadlocks.
    pumpMoveQueue();

    Fibre fibre = queue.pop();
    // Check for resumption
    if (fibre.cancel())
    {
      // Popped a cancelled fibre. Nothing to do.
      ++expired_count;
      continue;
    }

    const Resume resume = fibre.resume(epoch_time_s);
    if (resume.mode == ResumeMode::Expire || resume.mode == ResumeMode::Moved) [[unlikely]]
    {
      // Expired. All done.
      ++expired_count;
      continue;
    }

    if (resume.mode == ResumeMode::Exception) [[unlikely]]
    {
      // Propagate exception and expire.
      std::exception_ptr ex = fibre.exception();
      // TODO: Add an option to log instead of rethrowing.
      std::rethrow_exception(ex);
      ++expired_count;
      continue;  // Unreachable.
    }

    if (resume.reschedule) [[unlikely]]
    {
      const Priority reschedule = *resume.reschedule;
      const int32_t initial_priority = fibre.priority();
      if (initial_priority != reschedule.priority)
      {
        FibreQueue &new_queue = selectQueue(reschedule.priority, true);
        if (&new_queue != &queue)
        {
          // Update fibre priority and reschedule.
          fibre.__setPriority(reschedule.priority);
          new_queue.push(std::move(fibre), reschedule.position);
          // "expired" in this context.
          ++expired_count;
          continue;
        }
      }
    }

    // Push the fibre back for the next update.
    queue.push(std::move(fibre));
  }
}


void Scheduler::pumpMoveQueue()
{
  for (Fibre fibre = _move_queue.pop(); fibre.valid(); fibre = _move_queue.pop())
  {
    enqueue(std::move(fibre));
  }
}
}  // namespace morai
