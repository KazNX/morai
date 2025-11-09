#pragma once

#include "WeaverExport.hpp"

#include "Key.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace weaver
{
struct WEAVER_EXPORT Coord
{
  int x;
  int y;
};

struct WEAVER_EXPORT Size
{
  int width;
  int height;
};

struct WEAVER_EXPORT Viewport
{
  Coord origin;
  Size size;
};

enum class Color : uint8_t
{
  Black,
  Red,
  Green,
  Yellow,
  Blue,
  Magenta,
  Cyan,
  White,
};

enum class Modifier : uint8_t
{
  None = 0,
  Bold = 1 << 0,
  Italic = 1 << 1,
  Underline = 1 << 2,
  Blink = 1 << 3,
  Inverse = 1 << 4,
  Hidden = 1 << 5,
};

Modifier operator|(Modifier a, Modifier b);
Modifier operator&(Modifier a, Modifier b);
Modifier &operator|=(Modifier &a, Modifier b);
Modifier &operator&=(Modifier &a, Modifier b);

using Character = uint32_t;

inline Character character(uint16_t ch, Modifier modifiers = Modifier::None,
                           Color fg = Color::White, Color bg = Color::Black)
{
  return (static_cast<Character>(ch) & 0x0000FFFFu) |
         (static_cast<Character>(static_cast<uint8_t>(modifiers)) << 16u) |
         (static_cast<Character>(static_cast<uint8_t>(fg) & 0xfu) << 24u) |
         (static_cast<Character>(static_cast<uint8_t>(bg) & 0xfu) << 28u);
}

inline Character character(char ch, Modifier modifiers = Modifier::None, Color fg = Color::White,
                           Color bg = Color::Black)
{
  return character(static_cast<uint16_t>(ch), modifiers, fg, bg);
}

class WEAVER_EXPORT View
{
public:
  explicit View(const Viewport &viewport)
    : _viewport(viewport)
    , _data(viewport.size.width * viewport.size.height, 0)
  {}

  virtual ~View() = default;

  [[nodiscard]] Viewport viewport() const { return _viewport; }
  [[nodiscard]] std::span<const Character> data() const { return _data; }

  [[nodiscard]] Character character(Coord at) const
  {
    return _data.at(at.y * _viewport.size.width + at.x);
  }
  void setCharacter(Coord at, Character character)
  {
    _data.at(at.y * _viewport.size.width + at.x) = character;
  }


  void setString(Coord at, std::span<const Character> text, bool clear_eol = false);
  void setString(Coord at, std::string_view text, bool clear_eol = false);

  void clear() { std::fill(_data.begin(), _data.end(), 0); }

private:
  Viewport _viewport;
  std::vector<Character> _data;
};

class WEAVER_EXPORT Screen
{
public:
  Screen();
  ~Screen();

  [[nodiscard]] Size size() const { return viewport().size; }

  bool setViewport(const Viewport &viewport);
  [[nodiscard]] Viewport viewport() const;

  [[nodiscard]] std::optional<Character> input() const;

  View &addLayer(int32_t id, const Viewport &viewport);
  View &layer(int32_t id);

  void draw();

  /// Clear the screen buffer. Not effected until the next @c draw() call.
  void clear();

private:
  struct Layer
  {
    int32_t id;
    View view;
  };

  struct Imp;

  std::unique_ptr<Imp> _imp;
};


inline Modifier operator|(Modifier a, Modifier b)
{
  using IntType = std::underlying_type_t<Modifier>;
  return static_cast<Modifier>(static_cast<IntType>(a) | static_cast<IntType>(b));
}

inline Modifier operator&(Modifier a, Modifier b)
{
  using IntType = std::underlying_type_t<Modifier>;
  return static_cast<Modifier>(static_cast<IntType>(a) & static_cast<IntType>(b));
}

inline Modifier &operator|=(Modifier &a, Modifier b)
{
  a = a | b;
  return a;
}

inline Modifier &operator&=(Modifier &a, Modifier b)
{
  a = a & b;
  return a;
}
}  // namespace weaver
