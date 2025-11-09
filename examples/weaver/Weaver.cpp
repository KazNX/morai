#include "Weaver.hpp"

#include <curses.h>

#include <array>
#include <stdexcept>

namespace weaver
{
static_assert(sizeof(chtype) == sizeof(Character), "Character size mismatch");

// Key map: see https://pdcurses.org/docs/USERS.html
using KeyMapping = std::pair<chtype, Key>;
// constexpr std::array CHARACTER_MAP = {
//   KeyMapping{ KEY_BREAK, Key::Break },
//   KeyMapping{ KEY_DOWN, Key::Down },
//   KeyMapping{ KEY_UP, Key::Up },
//   KeyMapping{ KEY_LEFT, Key::Left },
//   KeyMapping{ KEY_RIGHT, Key::Right },
//   KeyMapping{ KEY_HOME, Key::Home },
//   KeyMapping{ KEY_BACKSPACE, Key::Backspace },
//   KeyMapping{ KEY_F0, Key::F0 },
//   KeyMapping{ KEY_F0 + 1, Key::F1 },
//   KeyMapping{ KEY_F0 + 2, Key::F2 },
//   KeyMapping{ KEY_F0 + 3, Key::F3 },
//   KeyMapping{ KEY_F0 + 4, Key::F4 },
//   KeyMapping{ KEY_F0 + 5, Key::F5 },
//   KeyMapping{ KEY_F0 + 6, Key::F6 },
//   KeyMapping{ KEY_F0 + 7, Key::F7 },
//   KeyMapping{ KEY_F0 + 8, Key::F8 },
//   KeyMapping{ KEY_F0 + 9, Key::F9 },
//   KeyMapping{ KEY_F0 + 10, Key::F10 },
//   KeyMapping{ KEY_F0 + 11, Key::F11 },
//   KeyMapping{ KEY_F0 + 12, Key::F12 },
//   KeyMapping{ KEY_F0 + 13, Key::F13 },
//   KeyMapping{ KEY_F0 + 14, Key::F14 },
//   KeyMapping{ KEY_F0 + 15, Key::F15 },
//   KeyMapping{ KEY_F0 + 16, Key::F16 },
//   KeyMapping{ KEY_CLEAR, Key::Clear },
//   KeyMapping{ KEY_EOS, Key::EOS },
//   KeyMapping{ KEY_EOL, Key::EOL },
//   KeyMapping{ KEY_SF, Key::ScrollForward },
//   KeyMapping{ KEY_SR, Key::ScrollReverse },
//   KeyMapping{ KEY_NPAGE, Key::PageDown },
//   KeyMapping{ KEY_PPAGE, Key::PageUp },
//   KeyMapping{ KEY_ENTER, Key::Enter },
//   KeyMapping{ KEY_PRINT, Key::Print },
//   KeyMapping{ KEY_COMMAND, Key::Cmd },
//   KeyMapping{ KEY_COPY, Key::Copy },
//   KeyMapping{ KEY_END, Key::End },
// };

void View::setString(Coord at, std::span<const Character> text, bool clear_eol)
{
  const int width = _viewport.size.width;
  for (size_t i = 0; i < text.size(); ++i)
  {
    const int x = at.x + static_cast<int>(i);
    const int y = at.y;
    if (x >= 0 && x < width && y >= 0 && y < _viewport.size.height)
    {
      _data.at(y * width + x) = text[i];
    }
  }

  if (clear_eol)
  {
    for (int x = at.x + static_cast<int>(text.size()); x < width; ++x)
    {
      _data.at(at.y * width + x) = 0;
    }
  }
}

void View::setString(Coord at, std::string_view text, bool clear_eol)
{
  const int width = _viewport.size.width;
  for (size_t i = 0; i < text.size(); ++i)
  {
    const int x = at.x + static_cast<int>(i);
    const int y = at.y;
    if (x >= 0 && x < width && y >= 0 && y < _viewport.size.height)
    {
      _data.at(y * width + x) = weaver::character(text.at(i));
    }
  }

  if (clear_eol)
  {
    for (int x = at.x + static_cast<int>(text.size()); x < width; ++x)
    {
      _data.at(at.y * width + x) = 0;
    }
  }
}

struct Screen::Imp
{
  using WindowPtr = std::unique_ptr<WINDOW, decltype(&delwin)>;
  std::vector<Layer> layers;
  std::unique_ptr<WINDOW, decltype(&delwin)> window{ newwin(0, 0, 0, 0), &delwin };

  Imp()
  {
    initscr();
    cbreak();
    noecho();
    window = WindowPtr{ newwin(0, 0, 0, 0), &delwin };
    keypad(window.get(), TRUE);
    nodelay(window.get(), TRUE);
    curs_set(0);
  }
};

Screen::Screen()
  : _imp(std::make_unique<Imp>())
{
  int width = getmaxx(_imp->window.get());
  int height = getmaxy(_imp->window.get());
  width = height;
}

Screen::~Screen() = default;

bool setViewport(const Viewport &viewport)
{
  return false;
}

Viewport Screen::viewport() const
{
  return { { getbegx(_imp->window.get()), getbegy(_imp->window.get()) },
           { getmaxx(_imp->window.get()), getmaxy(_imp->window.get()) } };
}

std::optional<Character> Screen::input() const
{
  const auto ch = wgetch(_imp->window.get());
  if (ch != ERR)
  {
    return static_cast<Character>(ch);
  }
  return std::nullopt;
}

View &Screen::addLayer(int32_t id, const Viewport &viewport)
{
  for (auto iter = _imp->layers.begin(); iter != _imp->layers.end(); ++iter)
  {
    if (id < iter->id)
    {
      return _imp->layers.emplace(iter, Layer{ id, View{ viewport } })->view;
    }
  }

  return _imp->layers.emplace_back(Layer{ id, View{ viewport } }).view;
}

View &Screen::layer(int32_t id)
{
  for (auto &layer : _imp->layers)
  {
    if (layer.id == id)
    {
      return layer.view;
    }
  }
  throw std::runtime_error("Layer not found");
}

void Screen::draw()
{
  wclear(_imp->window.get());  // Suboptimal
  int width = getmaxx(_imp->window.get());
  int height = getmaxy(_imp->window.get());
  for (const auto &layer : _imp->layers)
  {
    const int y = layer.view.viewport().origin.y;
    const int width = layer.view.viewport().size.width;
    for (int y = layer.view.viewport().origin.y; y < layer.view.viewport().origin.y + height; ++y)
    {
      mvwaddchnstr(_imp->window.get(), 0, y,
                   reinterpret_cast<const chtype *>(layer.view.data().data()) + y * width, width);
    }
  }
}


void Screen::clear()
{
  for (auto &layer : _imp->layers)
  {
    layer.view.clear();
  }
}
}  // namespace weaver
