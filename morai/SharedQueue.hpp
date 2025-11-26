#pragma once

#include "Fibre.hpp"

#include "MPMCQueue.hpp"

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

  /// Return the priority level for this queue. Not used by this object.
  [[nodiscard]] int32_t priority() const noexcept { return _priority; }

  /// Estimate the number of items in the queue. This may be inaccurate as other threads may modify
  /// the queue.
  [[nodiscard]] size_t size() const { return _queue.size(); }
  /// Check if the queue is empty. This may be inaccurate as other threads may modify the queue.
  [[nodiscard]] bool empty() const { return _queue.empty(); }

  /// Try push into the shared queue. This may fail when full.
  ///
  /// On success the @p fibre coroutine handle is moved out of the @p fibre object into the queue.
  /// On failure the @p fibre argument remains valid and the caller must take handle it
  /// appropriately.
  ///
  /// @param fibre The fibre to push into to the queue.
  /// @return True on success, in which case @p fibre becomes invalid. The fibre remains valid on
  /// failure and the caller must handle it appropriately.
  [[nodiscard]] bool tryPush(Fibre &fibre);

  /// Pop the next item off the queue.
  [[nodiscard]] Fibre pop();

  /// Clear the queue, destroying all contained fibres.
  void clear();

private:
  /// Stores @c Fibre internals rather than a @c Fibre so we can deal with @c try_push() failing.
  rigtorp::MPMCQueue<std::coroutine_handle<Fibre::promise_type>> _queue;
  int32_t _priority = 0;
};
}  // namespace morai
