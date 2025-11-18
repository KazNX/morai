#include "Scheduler.hpp"
#include <exception>
#include "arachne/Fibre.hpp"
#include "arachne/FibreQueue.hpp"

namespace arachne
{
Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

bool Scheduler::isRunning(const Id fibre_id) const noexcept
{
  return _fibre_queues[0].contains(fibre_id) || _fibre_queues[1].contains(fibre_id);
}

Id Scheduler::start(Fibre &&fibre, int32_t priority, std::string_view name)
{
  // Fibre creation assigned the ID. We need to store it before moving the fibre.
  const Id fibre_id = fibre.id();
  fibre.setName(name);
  fibre.__setPriority(priority);
  FibreQueue &fibres = activeQueue();
  fibres.push(std::move(fibre));
  return fibre_id;
}

bool Scheduler::cancel(const Id fibre_id)
{
  return _fibre_queues[0].cancel(fibre_id) || _fibre_queues[1].cancel(fibre_id);
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
  _fibre_queues[0].clear();
  _fibre_queues[1].clear();
}

void Scheduler::update(const double epoch_time_s)
{
  FibreQueue &update_queue = activeQueue();
  FibreQueue &next_queue = inactiveQueue();
  // Swap queues now for any pushes during update.
  swapQueues();

  _time.dt = epoch_time_s - _time.epoch_time_s;
  _time.epoch_time_s = epoch_time_s;
  while (!update_queue.empty())
  {
    Fibre fibre = update_queue.pop();
    // Check for resumption
    if (fibre.cancel())
    {
      // Popped a cancelled fibre. Nothing to do.
      continue;
    }

    const Resume resume = fibre.resume(epoch_time_s);
    if (resume.mode == ResumeMode::Expire) [[unlikely]]
    {
      // Expired. All done.
      continue;
    }

    if (resume.mode == ResumeMode::Exception) [[unlikely]]
    {
      // Propagate exception and expire.
      std::exception_ptr ex = fibre.exception();
      // TODO: Add an option to log instead of rethrowing.
      std::rethrow_exception(ex);
      continue;  // Unreachable.
    }

    if (resume.reschedule) [[unlikely]]
    {
      const Priority reschedule = *resume.reschedule;
      const int32_t initial_priority = fibre.priority();
      // Update fibre priority and reinsert based on that priority.
      fibre.__setPriority(reschedule.priority);
      next_queue.push(std::move(fibre), reschedule.position);
      continue;
    }

    // Push the fibre back for the next update.
    next_queue.push(std::move(fibre));
  }
}
}  // namespace arachne
