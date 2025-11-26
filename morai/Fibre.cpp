#include "Fibre.hpp"

namespace morai
{
void Fibre::Awaitable::await_suspend(std::coroutine_handle<promise_type> handle) noexcept
{
  handle.promise().frame.resumption = resumption;
}

void Fibre::FibreIdAwaitable::await_suspend(std::coroutine_handle<promise_type> handle) noexcept
{
  auto &promise = handle.promise();
  if (promise.frame.id != id)
  {
    promise.frame.resumption = wait([id = id]() { return !id.isRunning(); });
  }
  else
  {
    // Self await. Set no condition, just a yield.
    promise.frame.resumption = yield();
  }
}

void Fibre::RescheduleAwaitable::await_suspend(std::coroutine_handle<promise_type> handle) noexcept
{
  handle.promise().frame.reschedule = value;
}

std::atomic<IdValueType> Fibre::_next_id{ 0 };

Fibre::~Fibre()
{
  if (_handle)
  {
    // flagNotRunning();
    _handle.destroy();
  }
}

[[nodiscard]] Resume Fibre::resume(const double epoch_time_s) noexcept
{
  auto &promise = _handle.promise();
  const Resumption &resumption = promise.frame.resumption;
  if (done())
  {
    return { .mode = ResumeMode::Expire };
  }

  if (resumption.condition)
  {
    if (!resumption.condition() && (resumption.time_s <= 0 || epoch_time_s < resumption.time_s))
    {
      return { .mode = ResumeMode::Sleep };
    }
  }
  else if (epoch_time_s < resumption.time_s)
  {
    return { .mode = ResumeMode::Sleep };
  }

  // Resume will set promise.frame.resumption again so long as we haven't expired.
  // Only resume if we are not waiting on a move.
  promise.frame.resumption = {};
  if (!promise.frame.move_operation)
  {
    _handle.resume();
    if (promise.frame.exception)
    {
      return { .mode = ResumeMode::Exception };
    }

    if (_handle.done())
    {
      return { .mode = ResumeMode::Expire };
    }
  }

  // Check for move. This may may move a fibre immediately after it's last update - i.e.,
  // immediately after the co_await moveTo() statement. On failure, the fibre will remain with the
  // current scheduler. We must not update it, so the move_operation check around _handle.resume()
  // prevents update from the wrong scheduler.
  if (promise.frame.move_operation)
  {
    auto move_op = std::exchange(promise.frame.move_operation, {});
    // Note: the move operation will invalidate this object on success as it's content is moved
    // away. On failure the content is moved back here.
    if (move_op(*this))
    {
      return { ResumeMode::Moved };
    }
    // Move failed. Target queue may be full. Return ownership to this object and try again next
    // update.
    std::swap(promise.frame.move_operation, move_op);
    return { .mode = ResumeMode::Continue };
  }

  // Add the epoch time to the resumption value to set the correct resume time.
  if (promise.frame.resumption.time_s > 0)
  {
    promise.frame.resumption.time_s += epoch_time_s;
  }
  return { .mode = ResumeMode::Continue,
           .reschedule = std::exchange(promise.frame.reschedule, std::nullopt) };
}
}  // namespace morai
