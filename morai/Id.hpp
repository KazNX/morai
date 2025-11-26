#pragma once

#include <cstdint>
#include <memory>

namespace morai
{
using IdValueType = uint64_t;
constexpr auto InvalidFibreValue = ~static_cast<IdValueType>(0);

/// Id class for a fibre. Each fibre is uniquely identified by an Id.
///
/// The Id itself is a wrapper around a shared value which holds the actual id. Conceptually it can
/// be considered a shared pointer.
///
/// The @c ID does not represent a valid fibre if it's value is @c InvalidFibreValue - see
/// @c valid(). The @c Id can also be used to check if the fibre is still alive or has been
/// cleaned up and is no longer running - see @c isRunning().
class Id
{
public:
  static constexpr uint64_t RunningBit = 1u;

  Id() = default;
  /// Construct an @c Id with the given value and set its running state.
  ///
  /// @param value The id value. Note that the @c RunningBit must not be set to ensure a valid Id.
  /// In general only the @c Fibre class should set Id values.
  /// @param running Mark the fibre @c Id as running?
  explicit Id(IdValueType value, bool running = false)
    : ptr_(std::make_shared<IdValueType>(value))
  {
    setRunning(running);
  }
  ~Id() = default;

  Id(const Id &other) = default;
  Id(Id &&other) noexcept = default;
  Id &operator=(const Id &other) = default;
  Id &operator=(Id &&other) noexcept = default;

  /// Reports the @c Id value. This never reports the @c RunningBit.
  [[nodiscard]] uint64_t id() const noexcept
  {
    return (ptr_) ? (*ptr_ & ~RunningBit) : InvalidFibreValue;
  }

  /// Returns true if this represents a valid @c Id.
  [[nodiscard]] bool valid() const noexcept { return ptr_ && *ptr_ != InvalidFibreValue; }

  /// Returns true if the fibre associated with this @c Id is marked as running.
  [[nodiscard]] bool isRunning() const noexcept { return valid() && (*ptr_ & RunningBit); }

private:
  /// Set the running state of this @c Id.
  void setRunning(bool is_running) noexcept
  {
    if (valid())
    {
      if (is_running)
      {
        *ptr_ |= RunningBit;
      }
      else
      {
        *ptr_ &= ~RunningBit;
      }
    }
  }

  friend class Fibre;

  std::shared_ptr<IdValueType> ptr_;
};

/// @c Id equality operator.
inline bool operator==(const Id &lhs, const Id &rhs) noexcept
{
  return lhs.id() == rhs.id();
}

/// @c Id inequality operator.
inline bool operator!=(const Id &lhs, const Id &rhs) noexcept
{
  return lhs.id() != rhs.id();
}
}  // namespace morai
