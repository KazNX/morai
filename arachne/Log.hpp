#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace arachne::log
{
enum class Level
{
  Debug,
  Info,
  Warn,
  Error,
  Fatal
};

using LogHook = std::function<void(Level, std::string_view)>;

void setHook(std::function<void(Level, std::string_view)> hook);
void clearHook();

void log(Level level, std::string_view msg);

inline void debug(std::string_view msg)
{
  log(Level::Debug, msg);
}

inline void info(std::string_view msg)
{
  log(Level::Info, msg);
}

inline void warn(std::string_view msg)
{
  log(Level::Warn, msg);
}

inline void error(std::string_view msg)
{
  log(Level::Error, msg);
}

inline void fatal(std::string_view msg)
{
  log(Level::Fatal, msg);
  throw std::runtime_error(std::string{ msg });
}
}  // namespace arachne::log
