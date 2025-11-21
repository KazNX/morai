#pragma once

#include <cstdint>
#include <memory>

namespace arachne
{
using IdValueType = uint64_t;
constexpr auto InvalidFibreValue = ~static_cast<IdValueType>(0);

class Id
{
public:
  static constexpr uint64_t RunningBit = 1u;

  Id() = default;
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

  [[nodiscard]] uint64_t id() const noexcept
  {
    return (ptr_) ? (*ptr_ & ~RunningBit) : InvalidFibreValue;
  }

  [[nodiscard]] bool valid() const noexcept { return ptr_ && *ptr_ != InvalidFibreValue; }

  [[nodiscard]] bool isRunning() const noexcept { return valid() && (*ptr_ & RunningBit); }

private:
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

inline bool operator==(const Id &lhs, const Id &rhs) noexcept
{
  return lhs.id() == rhs.id();
}
}  // namespace arachne
