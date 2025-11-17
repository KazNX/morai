#pragma once

#include "Fibre.hpp"
#include "Resumption.hpp"

namespace arachne
{
/// A single threaded fibre queue with priority insertion.
///
/// This queue is for single threaded
class FibreQueue
{
public:
  explicit FibreQueue(uint64_t capacity = 1024u);
  ~FibreQueue();

  FibreQueue(FibreQueue &&other) noexcept;
  FibreQueue &operator=(FibreQueue &&other) noexcept;

  FibreQueue(const FibreQueue &) = delete;
  FibreQueue &operator=(const FibreQueue &) = delete;

  [[nodiscard]] size_t size() const
  {
    if (_head >= _tail)
    {
      return _head - _tail;
    }
    return _buffer.size() + _head - _tail;
  }

  [[nodiscard]] bool empty() const { return _head == _tail; }

  [[nodiscard]] bool contains(const Id &id) const;

  void push(Fibre &&fibre, PriorityPosition position = PriorityPosition::Back);

  /// Pop the next item off the queue.
  [[nodiscard]] Fibre pop();

  bool cancel(const Id &id);

  void clear();

private:
  [[nodiscard]] uint64_t nextIndex(const uint64_t index) const
  {
    return (index + 1) & (_buffer.size() - 1);
  }

  [[nodiscard]] uint64_t priorIndex(const uint64_t index) const
  {
    return (index - 1) & (_buffer.size() - 1);
  }

  void grow();

  uint64_t _head = 0;
  uint64_t _tail = 0;
  std::vector<Fibre> _buffer{};
};
}  // namespace arachne
