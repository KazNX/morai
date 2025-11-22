#include "Fibre.hpp"

namespace morai
{
void Fibre::Awaitable::await_suspend(std::coroutine_handle<promise_type> handle) noexcept
{
  handle.promise().frame.resumption = resumption;
}

void Fibre::FibreIdAwaitable::await_suspend(std::coroutine_handle<promise_type> handle) noexcept
{
  handle.promise().frame.resumption = wait([id = id]() {
    //
    return !id.isRunning();
  });
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
    flagNotRunning();
    _handle.destroy();
  }
}

[[nodiscard]] Resume Fibre::resume(const double epoch_time_s) noexcept
{
  auto &promise = _handle.promise();
  const Resumption &resumption = promise.frame.resumption;
  if (done())
  {
    flagNotRunning();
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

  // Resume will set promise.value again so long as we haven't expired.
  promise.frame.resumption = {};
  _handle.resume();
  if (_handle.promise().frame.exception)
  {
    flagNotRunning();
    return { .mode = ResumeMode::Exception };
  }

  if (_handle.done())
  {
    flagNotRunning();
    return { .mode = ResumeMode::Expire };
  }

  // Check for move
  if (_handle.promise().frame.move_operation)
  {
    auto move_op = std::exchange(_handle.promise().frame.move_operation, {});
    move_op(std::move(*this));
    return { ResumeMode::Moved };
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
