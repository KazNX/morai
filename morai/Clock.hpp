#pragma once

#include <atomic>
#include <chrono>
#include <functional>

namespace morai
{
/// Class used to track epoch time for scheduler classes.
///
/// This class implements the @c Clock interface for @c Scheduler and @c ThreadPool schedulers such
/// that they can track either real time, or a user specified time. The default construction uses
/// the @c std::chrono::steady_clock to track real time with the base @p epoch() set to zero on the
/// first call to request the time.
///
/// A custom clock is implemented by setting the @c TimeFunction on construction and allows tracking
/// an arbitrary time source - e.g., simulation time, a replay clock or a mock clock.
///
/// The @c epoch() always reports the last stored time value (nominally seconds), while @c update()
/// invokes the @c TimeFunction then reports the epoch time.
///
/// The internal time is stored as an atomic 64-bit integer using the @c quantisation() value to
/// scale from floating point seconds into a fixed point representation. The fixed point value is
/// accessible via @c tick(). The default quantisation 1us per tick.
///
/// The @c update(), @c epoch() and @c tick() functions are thread safe, provided the
/// @c TimeFunction is threadsafe.
class Clock
{
public:
  /// Default quantisation between floating point seconds and the fixed point @c tick() value.
  static constexpr double DefaultQuantisation = 1e-6;

  /// Signature for the time evaluation function.
  using TimeFunction = std::function<double()>;

  /// Create a new clock object with a custom time function.
  /// @param now The custom time function.
  /// @param quantisation The quantisation value for each tick.
  explicit Clock(TimeFunction now, const double quantisation = DefaultQuantisation) noexcept
    : _quantisation{ quantisation }
    , _now{ std::move(now) }
  {}

  /// Create a new clock object using the default time function and the given quantisation.
  /// @param quantisation The quantisation value for each tick.
  explicit Clock(const double quantisation = DefaultQuantisation) noexcept
    : Clock{ Clock::steady_clock_time_function, quantisation }
  {}

  Clock(Clock &&other) noexcept
    : _time{ other._time.load() }
    , _quantisation{ other._quantisation }
    , _now{ std::move(other._now) }
  {}

  Clock(const Clock &other) noexcept
    : _time{ other._time.load() }
    , _quantisation{ other._quantisation }
    , _now{ other._now }
  {}

  Clock &operator=(Clock &&clock) = delete;
  Clock &operator=(const Clock &) = delete;

  ~Clock() = default;

  /// Get the quantisation value set on construction.
  [[nodiscard]] double quantisation() const noexcept { return _quantisation; }

  /// Get the time function set on construction.
  [[nodiscard]] TimeFunction timeFunction() const noexcept { return _now; }

  /// Get the current epoch time from the last @c update() call.
  [[nodiscard]] double epoch() const noexcept
  {
    return static_cast<double>(_time.load()) * _quantisation;
  }

  /// Get the current tick value from the last @c update() call.
  [[nodiscard]] uint64_t tick() const noexcept { return _time.load(); }

  /// Update the time value by invoking the @c TimeFunction and updating the @c epoch().
  /// @return The new epoch time.
  double update()
  {
    const double now_s = _now();
    const uint64_t now = static_cast<uint64_t>(now_s / _quantisation);
    _time.store(now);
    return now_s;
  }

  /// Implements the default time function using @c std::chrono::steady_clock.
  /// @return The epoch time (seconds) since the first call to this function.
  static double steady_clock_time_function() noexcept
  {
    using namespace std::chrono;
    static const auto base_time = steady_clock::now();
    const auto elapsed = (steady_clock::now() - base_time);
    return duration<double>(elapsed).count();
  }

private:
  std::atomic_uint64_t _time{ 0 };
  const double _quantisation = DefaultQuantisation;
  TimeFunction _now;
};
}  // namespace morai
