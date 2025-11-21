#include "Log.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <format>

namespace arachne::log
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
}

std::function<void(Level, std::string_view)> hook = &default_hook;
}  // namespace

void setHook(std::function<void(Level, std::string_view)> new_hook)
{
  hook = new_hook;
}

void log(const Level level, std::string_view msg)
{
  hook(level, std::string{ msg });
}
}  // namespace arachne::log
