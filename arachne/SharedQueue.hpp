#pragma once

#include "Fibre.hpp"

#include "MPMCQueue.hpp"

#include <atomic>

namespace arachne
{
/// A multi-threaded, multi-producer, multi-consumer, lock free queue.
///
/// @par Implementation
///
/// Uses [Erik Rigtorp mpmc queue](https://github.com/rigtorp/MPMCQueue) under MIT license.
class SharedQueue
{
public:
  explicit SharedQueue(int32_t priority, uint32_t capacity);
  ~SharedQueue();

  SharedQueue(SharedQueue &&other) noexcept = delete;
  SharedQueue &operator=(SharedQueue &&other) noexcept = delete;

  SharedQueue(const SharedQueue &) = delete;
  SharedQueue &operator=(const SharedQueue &) = delete;

  [[nodiscard]] int32_t priority() const noexcept { return _priority; }

  [[nodiscard]] size_t size() const { return _queue.size(); }
  [[nodiscard]] bool empty() const { return _queue.empty(); }

  void push(Fibre &&fibre);

  /// Pop the next item off the queue.
  [[nodiscard]] Fibre pop();

  bool cancel(const Id &id);

  void clear();

private:
  rigtorp::MPMCQueue<Fibre> _queue;
  int32_t _priority = 0;
};
}  // namespace arachne
