#include "SharedQueue.hpp"
namespace morai
{
SharedQueue::SharedQueue(int32_t priority, uint32_t capacity)
  : _queue{ capacity }
  , _priority{ priority }
{}

SharedQueue::~SharedQueue()
{
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
  return { handle, id };
}

Fibre SharedQueue::pop()
{
  std::coroutine_handle<Fibre::promise_type> handle;
  _queue.try_pop(handle);
  if (handle)
  {
    return { handle, handle.promise().frame.id };
  }
  return {};
}

void SharedQueue::clear()
{
  while (!_queue.empty())
  {
    std::ignore = pop();
  }
}
}  // namespace morai
