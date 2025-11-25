#include "SharedQueue.hpp"
namespace morai
{
SharedQueue::SharedQueue(int32_t priority, uint32_t capacity)
  : _queue{ capacity }
  , _priority{ priority }
{}

SharedQueue::~SharedQueue()
{
  // We have to pop all items in order to properly destroy them.
  clear();
}

Fibre SharedQueue::push(Fibre &&fibre)
{
  auto handle = fibre.__release();
  // Must copy the idea to avoid self move.
  Id id = handle.promise().frame.id;
  if (_queue.try_emplace(handle))
  {
    return {};
  }
  return { handle };
}

Fibre SharedQueue::pop()
{
  std::coroutine_handle<Fibre::promise_type> handle;
  _queue.try_pop(handle);
  if (handle)
  {
    return { handle };
  }
  return {};
}

void SharedQueue::clear()
{
  // We must explicitly destroy each handle since normally it's not wrapped in a Fibre which
  // normally does this.
  std::coroutine_handle<Fibre::promise_type> handle;
  while (_queue.try_pop(handle))
  {
    handle.destroy();
  }
}
}  // namespace morai
