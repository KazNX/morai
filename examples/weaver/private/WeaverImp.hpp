#pragma once

#include "weaver/Weaver.hpp"

#include <curses.h>

#include <vector>
#include <functional>

namespace weaver
{
struct Layer
{
  int32_t id;
  View view;
};

struct ScreenImp
{
  using WindowPtr = std::unique_ptr<WINDOW, decltype(&delwin)>;
  std::vector<Layer> layers;
  std::unique_ptr<WINDOW, decltype(&delwin)> window{ newwin(0, 0, 0, 0), &delwin };
  uint8_t next_colour_pair = 1;
  std::function<void(int)> input_hook;

  ScreenImp()
  {
    initscr();
    cbreak();
    noecho();
    timeout(0);
    start_color();
    window = WindowPtr{ newwin(0, 0, 0, 0), &delwin };
    keypad(window.get(), TRUE);
    nodelay(window.get(), TRUE);
    curs_set(0);
  }
};
}  // namespace weaver
