#pragma once

#include "Fibre.hpp"

#include "MPMCQueue.hpp"

#include <atomic>
#include <coroutine>

namespace morai
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

  /// Try push into the shared queue. This may fail when full in which case the return value must
  /// be captured. This can be used to address potential deadlock issues.
  ///
  /// @param fibre The fibre to move to the queue.
  /// @return An invalid fibre - @c Fibre::valid() is false - on success, or the same fibre back
  /// on failure (queue full).
  [[nodiscard]] Fibre push(Fibre &&fibre);

  /// Pop the next item off the queue.
  [[nodiscard]] Fibre pop();

  bool cancel(const Id &id);

  void clear();

private:
  /// Stores @c Fibre internals rather than a @c Fibre so we can deal with @c try_push() failing.
  rigtorp::MPMCQueue<std::coroutine_handle<Fibre::promise_type>> _queue;
  int32_t _priority = 0;
};
}  // namespace morai
