#include "FibreQueue.hpp"

#include <algorithm>
#include <ranges>
#include "Common.hpp"

namespace arachne
{
FibreQueue::FibreQueue(uint64_t capacity)
{
  capacity = std::max<uint64_t>(capacity, 16u);
  capacity = nextPowerOfTwo(capacity);
  _buffer.resize(capacity);
}

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
  // TODO: primary insertion.
  uint64_t next_head = nextIndex(_head);
  if (next_head == _tail)
  {
    grow();
    next_head = nextIndex(_head);
  }

  // Handle priority insertion.
  if (empty() || fibre.priority() > _buffer.at(priorIndex(_head)).priority() ||
      fibre.priority() == _buffer.at(priorIndex(_head)).priority() &&
        position == PriorityPosition::Back)
  {
    // Normal insertion at head.
    _buffer.at(_head) = std::move(fibre);
    _head = next_head;
    return;
  }

  // Priority insertion. Find insertion point.
  uint64_t insert_index = _tail;
  while (insert_index != _head)
  {
    if (fibre.priority() < _buffer.at(insert_index).priority() ||
        fibre.priority() == _buffer.at(insert_index).priority() &&
          position == PriorityPosition::Front)
    {
      break;
    }
    insert_index = nextIndex(insert_index);
  }

  // Shift items up to make space.
  uint64_t current_index = _head;
  while (current_index != insert_index)
  {
    uint64_t prior_index = priorIndex(current_index);
    std::swap(_buffer.at(current_index), _buffer.at(prior_index));
    current_index = prior_index;
  }

  _buffer.at(insert_index) = std::move(fibre);
  _head = next_head;
}

Fibre FibreQueue::pop()
{
  if (empty())
  {
    return {};
  }

  Fibre fibre = std::move(_buffer[_tail]);
  _tail = (_tail + 1) & (_buffer.size() - 1);
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
