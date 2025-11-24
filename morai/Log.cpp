#include "Log.hpp"

#include <atomic>
#include <algorithm>
#include <array>
#include <iostream>
#include <format>

namespace morai::log
{
namespace
{
constexpr std::array<std::string_view, 5> level_strings = { "Debug", "Info", "Warn", "Error",
                                                            "Fatal" };

std::string_view level_name(const Level level)
{
  return level_strings[std::clamp(static_cast<int>(level), 0, 4)];
}

void default_hook(const Level level, std::string_view msg)
{
  std::cout << std::format("[{}]: {}", level_name(level), msg) << '\n';
  if (level == Level::Fatal) [[unlikely]]
  {
    throw std::runtime_error(std::string{ msg });
  }
}

std::function<void(Level, std::string_view)> hook = &default_hook;
std::atomic_int active_level{ static_cast<int>(Level::Info) };
}  // namespace

void setActiveLevel(Level level)
{
  active_level = static_cast<int>(level);
}

Level activeLevel()
{
  return static_cast<Level>(active_level.load());
}

void setHook(std::function<void(Level, std::string_view)> new_hook)
{
  hook = new_hook;
}

void clearHook()
{
  hook = &default_hook;
}

void log(const Level level, std::string_view msg)
{
  if (static_cast<int>(level) < static_cast<int>(activeLevel()))
  {
    return;
  }

  hook(level, std::string{ msg });
}
}  // namespace morai::log
