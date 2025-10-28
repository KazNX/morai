module arachne;

import std;

namespace arachne
{

void Fibre::Awaitable::await_suspend(std::coroutine_handle<promise_type> handle) noexcept
{
  handle.promise().resumption = resumption;
}

Fibre::~Fibre()
{
  if (_handle)
  {
    _handle.destroy();
  }
}

[[nodiscard]] Resume Fibre::resume(const double epoch_time_s) noexcept
{
  auto &promise = _handle.promise();
  const Resumption resumption = std::exchange(promise.resumption, {});
  if (done())
  {
    return Resume::Expire;
  }

  if (resumption.condition)
  {
    if (!resumption.condition() && (resumption.time_s <= 0 || epoch_time_s < resumption.time_s))
    {
      return Resume::Sleep;
    }
  }
  else if (epoch_time_s < resumption.time_s)
  {
    return Resume::Sleep;
  }

  // Resume will set promise.value again so long as we haven't expired.
  _handle.resume();
  if (_handle.promise().exception)
  {
    std::rethrow_exception(_handle.promise().exception);
  }

  if (_handle.done())
  {
    return Resume::Expire;
  }

  // Add the epoch time to the resumption value to set the correct resume time.
  if (promise.resumption.time_s > 0)
  {
    promise.resumption.time_s += epoch_time_s;
  }
  return Resume::Continue;
}
}  // namespace arachne
