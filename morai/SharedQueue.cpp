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

void SharedQueue::push(Fibre &&fibre)
{
  _queue.emplace(std::move(fibre));
}

Fibre SharedQueue::pop()
{
  Fibre fibre;
  _queue.try_pop(fibre);
  return fibre;
}

void SharedQueue::clear()
{
  while (!_queue.empty())
  {
    std::ignore = pop();
  }
}
}  // namespace morai
