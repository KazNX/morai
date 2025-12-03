#include "Weaver.hpp"

#include "private/WeaverImp.hpp"

#include <curses.h>

#include <stdexcept>

namespace weaver
{
static_assert(sizeof(chtype) == sizeof(Character), "Character size mismatch");

namespace
{
chtype convertForDraw(const Character ch)
{
  const auto mods = static_cast<Modifier>((ch >> 16u) & 0xFFu);
  const auto colour = static_cast<chtype>((ch >> 24u) & 0xFFu);
  auto result = static_cast<chtype>(ch & 0x0000FFFFu);

  if (colour)
  {
    result |= COLOR_PAIR(colour);
  }

  if ((mods & Modifier::Bold) == Modifier::Bold)
  {
    result |= A_BOLD;
  }
  if ((mods & Modifier::Italic) == Modifier::Italic)
  {
    result |= A_ITALIC;
  }
  if ((mods & Modifier::Underline) == Modifier::Underline)
  {
    result |= A_UNDERLINE;
  }
  if ((mods & Modifier::Blink) == Modifier::Blink)
  {
    result |= A_BLINK;
  }
  if ((mods & Modifier::Inverse) == Modifier::Inverse)
  {
    result |= A_REVERSE;
  }
  if ((mods & Modifier::Hidden) == Modifier::Hidden)
  {
    result |= A_INVIS;
  }

  return result;
}
}  // namespace

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

Screen::Screen()
  : _imp(std::make_unique<ScreenImp>())
{}

Screen::~Screen() = default;

bool setViewport([[maybe_unused]] const Viewport &viewport)
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
  if (_imp->input_hook)
  {
    const int ch = wgetch(_imp->window.get());
    _imp->input_hook(ch);
  }
  wclear(_imp->window.get());  // Suboptimal
  int width = getmaxx(_imp->window.get());
  int height = getmaxy(_imp->window.get());
  for (const auto &layer : _imp->layers)
  {
    for (int y = layer.view.viewport().origin.y; y < layer.view.viewport().origin.y + height; ++y)
    {
      for (int x = layer.view.viewport().origin.x; x < layer.view.viewport().origin.x + width; ++x)
      {
        const auto ch = layer.view.character({ x, y });
        if (ch != 0)
        {
          mvwaddch(_imp->window.get(), y, x, convertForDraw(ch));
        }
      }
    }
  }
  wrefresh(_imp->window.get());
}

void Screen::clear()
{
  for (auto &layer : _imp->layers)
  {
    layer.view.clear();
  }
}
}  // namespace weaver
