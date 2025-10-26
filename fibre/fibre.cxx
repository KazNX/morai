module fibre;

import std;

namespace fibre
{

Fibre::~Fibre()
{
  if (_handle)
  {
    _handle.destroy();
  }
}

[[nodiscard]] Yield Fibre::resume() noexcept
{
  _handle.resume();
  if (_handle.promise().exception)
  {
    std::rethrow_exception(_handle.promise().exception);
  }
  return (!_handle.done()) ? _handle.promise().value : Yield{};
}
}  // namespace fibre
