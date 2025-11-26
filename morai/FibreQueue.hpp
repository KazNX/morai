#pragma once

#include "Fibre.hpp"
#include "Resumption.hpp"

namespace morai
{
/// A single threaded fibre queue with priority insertion.
///
/// A fibre queue is a primarily a FIFO queue. @c Fibre objects are *moved* into the queue using
/// @c push() and removed with @c pop(). The queue will grow as required as more fibres are pushed.
/// The number of items is represented by @c size() and @c empty() tests if the fibre has items or
/// not.
///
/// None of these operations are threadsafe. See @c SharedQueue for a thread safe (fixed size)
/// queue.
///
/// Typical usage is for a scheduler to @c push() new fibres added to that scheduler, first
/// selecting a queue of the appropriate @c priority(). During a scheduler update, the queue is
/// continually popped - @c pop() - until it returns a an invalid @c Fibre - see @c Fibre::valid().
///
/// Copy operations are disabled because @c Fibre objects are move-only.
class FibreQueue
{
public:
  /// Create a fibre of the given @p priority and initial @p capacity.
  ///
  /// @param priority The queue priority. This priority is not used by the queue itself.
  explicit FibreQueue(int32_t priority, uint32_t capacity = 1024u);
  ~FibreQueue();

  FibreQueue(FibreQueue &&other) noexcept;
  FibreQueue &operator=(FibreQueue &&other) noexcept;

  FibreQueue(const FibreQueue &) = delete;
  FibreQueue &operator=(const FibreQueue &) = delete;

  /// Returns the priority of this queue. This is not used by the queue.
  [[nodiscard]] int32_t priority() const noexcept { return _priority; }

  /// Return the number of items in the queue.
  [[nodiscard]] std::size_t size() const
  {
    if (_head >= _tail)
    {
      return _head - _tail;
    }
    return _buffer.size() + _head - _tail;
  }

  /// Returns true if the queue is empty.
  [[nodiscard]] bool empty() const { return _head == _tail; }

  /// Returns true if the queue contains a fibre with the given @p id.
  /// @param id The @c Id to search for. An invalid @c Id always returns false.
  [[nodiscard]] bool contains(const Id &id) const;

  /// Position to insert a fibre into the queue.
  /// @param fibre The fibre to move into the queue.
  /// @param position @c PriorityPosition::Back to insert at the back of the queue (FIFO) or
  /// @c PriorityPosition::Front to insert at the front of the queue (LIFO).
  void push(Fibre &&fibre, PriorityPosition position = PriorityPosition::Back);

  /// Pop the next item off the queue.
  [[nodiscard]] Fibre pop();

  /// Cancel a fibre with the given @p id. The fibre immediately terminates.
  /// @param id The @c Id of the fibre to cancel.
  /// @return True if the fibre was cancelled.
  [[nodiscard]] bool cancel(const Id &id);

  /// Clear all fibres from the queue.
  void clear();

private:
  [[nodiscard]] bool full() const { return nextIndex(_head) == _tail; }

  [[nodiscard]] uint32_t nextIndex(const uint32_t index) const
  {
    return (index + 1) & (_buffer.size() - 1);
  }

  [[nodiscard]] uint32_t priorIndex(const uint32_t index) const
  {
    return (index - 1) & (_buffer.size() - 1);
  }

  void grow();

  uint32_t _head = 0;
  uint32_t _tail = 0;
  std::vector<Fibre> _buffer{};
  int32_t _priority = 0;
};
}  // namespace morai
