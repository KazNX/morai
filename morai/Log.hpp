#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

/// Logging interface. Provides a @c LogHook which can be set via @c setHook(). All log calls go via
/// this hook. The default logs to stdout.
namespace morai::log
{
/// Logging level options.
enum class Level : int8_t
{
  Debug,
  Info,
  Warn,
  Error,
  Fatal
};

/// Log hook signature.
using LogHook = std::function<void(Level, std::string_view)>;

/// Set the active level. The hook is not invoked for lower level calls.
void setActiveLevel(Level level);
/// Get the active log level.
Level activeLevel();

/// Set the log hook. Not threadsafe. Should only be called on startup.
void setHook(std::function<void(Level, std::string_view)> hook);
/// Restore the default log hook. Not threadsafe.
void clearHook();

/// Log a @p msg at the specified @p level.
void log(Level level, std::string_view msg);

/// Log @p msg at @c Level::Debug.
inline void debug(std::string_view msg)
{
  log(Level::Debug, msg);
}

/// Log @p msg at @c Level::Info.
inline void info(std::string_view msg)
{
  log(Level::Info, msg);
}

/// Log @p msg at @c Level::Warn.
inline void warn(std::string_view msg)
{
  log(Level::Warn, msg);
}

/// Log @p msg at @c Level::Error.
inline void error(std::string_view msg)
{
  log(Level::Error, msg);
}

/// Log @p msg at @c Level::Fatal - the default hook throws an exception.
inline void fatal(std::string_view msg)
{
  log(Level::Fatal, msg);
}
}  // namespace morai::log
