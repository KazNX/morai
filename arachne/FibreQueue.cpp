#include "FibreQueue.hpp"

#include <algorithm>
#include <ranges>
#include "Common.hpp"

namespace arachne
{
FibreQueue::FibreQueue(int32_t priority, uint64_t capacity)
  : _priority(priority)
{
  capacity = std::max<uint64_t>(capacity, 16u);
  capacity = nextPowerOfTwo(capacity);
  _buffer.resize(capacity);
}

FibreQueue::FibreQueue(FibreQueue &&other) noexcept = default;
FibreQueue &FibreQueue::operator=(FibreQueue &&other) noexcept = default;

FibreQueue::~FibreQueue()
{
  clear();
}

bool FibreQueue::contains(const Id &id) const
{
  if (empty())
  {
    return false;
  }

  for (uint64_t i = _tail; i != _head; i = nextIndex(i))
  {
    if (_buffer.at(i).id() == id)
    {
      return true;
    }
  }
  return false;
}

void FibreQueue::push(Fibre &&fibre, PriorityPosition position)
{
  if (full())
  {
    grow();
  }

  if (position == PriorityPosition::Back)
  {
    const uint64_t insert_index = nextIndex(_head);

    _buffer.at(_head) = std::move(fibre);
    _head = insert_index;
    return;
  }

  // Tail/front insertion.
  uint64_t insert_index = priorIndex(_tail);
  _buffer.at(insert_index) = std::move(fibre);
  _tail = insert_index;
}

Fibre FibreQueue::pop()
{
  if (empty())
  {
    return {};
  }

  Fibre fibre = std::move(_buffer[_tail]);
  _tail = nextIndex(_tail);
  return fibre;
}

void FibreQueue::grow()
{
  std::vector<Fibre> new_buffer(_buffer.size() * 2);
  uint64_t new_tail = 0u;
  while (!empty())
  {
    new_buffer.at(new_tail++) = pop();
  }
  _buffer.clear();

  std::swap(_buffer, new_buffer);
  _head = 0u;
  _tail = new_tail;
}


bool FibreQueue::cancel(const Id &id)
{
  if (id.id == InvalidFibreValue)
  {
    return false;
  }

  auto iter = std::ranges::find_if(_buffer, [&id](const Fibre &fibre) { return fibre.id() == id; });
  if (iter == _buffer.end())
  {
    return false;
  }

  Fibre replace;
  std::swap(*iter, replace);
  return true;
}

void FibreQueue::clear()
{
  _head = 0;
  _tail = 0;
  // Force fibre cleanup by clearing the buffer. Resize to capacity to restore usage.
  _buffer.clear();
  _buffer.resize(_buffer.capacity());
}
}  // namespace arachne
