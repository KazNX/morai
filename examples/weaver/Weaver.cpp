#include "Weaver.hpp"

#include <curses.h>

#include <array>
#include <stdexcept>

namespace weaver
{
static_assert(sizeof(chtype) == sizeof(Character), "Character size mismatch");

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
  uint8_t next_colour_pair = 1;

  Imp()
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

uint8_t Screen::defineColour(Colour fg, Colour bg)
{
  const auto id = _imp->next_colour_pair++;
  init_pair(id, static_cast<short>(fg), static_cast<short>(bg));
  return id;
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
  wgetch(_imp->window.get());
  wclear(_imp->window.get());  // Suboptimal
  int width = getmaxx(_imp->window.get());
  int height = getmaxy(_imp->window.get());
  for (const auto &layer : _imp->layers)
  {
    const int y = layer.view.viewport().origin.y;
    const int width = layer.view.viewport().size.width;
    for (int y = layer.view.viewport().origin.y; y < layer.view.viewport().origin.y + height; ++y)
    {
      for (int x = layer.view.viewport().origin.x; x < layer.view.viewport().origin.x + width; ++x)
      {
        const auto ch = layer.view.character({ x, y });
        if (ch != 0)
        {
          mvwaddch(_imp->window.get(), y, x, static_cast<chtype>(ch));
        }
      }
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
