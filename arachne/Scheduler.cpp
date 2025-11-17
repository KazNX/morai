#include "Scheduler.hpp"
#include <exception>

namespace arachne
{
Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

bool Scheduler::isRunning(const Id fibre_id) const noexcept
{
  return _fibres.contains(fibre_id);
}

Id Scheduler::start(Fibre &&fibre, int32_t priority, std::string_view name)
{
  // Fibre creation assigned the ID. We need to store it before moving the fibre.
  const Id fibre_id = fibre.id();
  fibre.setName(name);
  fibre.__setPriority(priority);
  _fibres.push(std::move(fibre));
  return fibre_id;
}

bool Scheduler::cancel(const Id fibre_id)
{
  return _fibres.cancel(fibre_id);
}

std::size_t Scheduler::cancel(std::span<const Id> fibre_ids)
{
  std::size_t removed = 0;
  for (const Id &id : fibre_ids)
  {
    removed += !!_fibres.cancel(id);
  }
  return removed;
}

void Scheduler::cancelAll()
{
  _fibres.clear();
}

void Scheduler::update(const double epoch_time_s)
{
  size_t fibre_count = _fibres.size();

  _time.dt = epoch_time_s - _time.epoch_time_s;
  _time.epoch_time_s = epoch_time_s;
  for (size_t i = 0; i < fibre_count && !_fibres.empty(); ++i)
  {
    auto fibre = _fibres.pop();
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
      _fibres.push(std::move(fibre), reschedule.position);
      // Update fibre count (end point) if scheduling to a later position.
      if (reschedule.priority > initial_priority ||
          reschedule.priority == initial_priority && reschedule.position == PriorityPosition::Back)
      {
        ++fibre_count;
      }
      continue;
    }

    // Push the fibre back for the next update.
    _fibres.push(std::move(fibre));
  }
}
}  // namespace arachne
