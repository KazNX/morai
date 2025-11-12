#pragma once

#include "WeaverExport.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace weaver
{
struct WEAVER_EXPORT Coord
{
  int x = 0;
  int y = 0;
};

inline Coord operator+(const Coord &a, const Coord &b)
{
  return Coord{ a.x + b.x, a.y + b.y };
}

inline Coord &operator+=(Coord &a, const Coord &b)
{
  a = a + b;
  return a;
}

inline Coord operator-(const Coord &a, const Coord &b)
{
  return Coord{ a.x - b.x, a.y - b.y };
}

inline Coord &operator-=(Coord &a, const Coord &b)
{
  a - b;
  return a;
}

inline Coord operator*(const Coord &a, const int scalar)
{
  return Coord{ a.x * scalar, a.y * scalar };
}

inline Coord &operator*=(Coord &a, const int scalar)
{
  a = a * scalar;
  return a;
}

inline Coord operator*(const int scalar, const Coord &a)
{
  return Coord{ a.x * scalar, a.y * scalar };
}

inline Coord operator/(const Coord &a, const int scalar)
{
  return Coord{ a.x / scalar, a.y / scalar };
}

inline Coord &operator/=(Coord &a, const int scalar)
{
  a = a / scalar;
  return a;
}

struct WEAVER_EXPORT Size
{
  int width = 0;
  int height = 0;
};

struct WEAVER_EXPORT Viewport
{
  Coord origin{};
  Size size{};
};

inline Coord clamp(const Viewport &view, const Coord &coord)
{
  return Coord{ .x = std::clamp(coord.x, view.origin.x, view.origin.x + view.size.width - 1),
                .y = std::clamp(coord.y, view.origin.y, view.origin.y + view.size.height - 1) };
}

inline bool contains(const Viewport &view, const Coord &coord)
{
  return coord.x >= view.origin.x && coord.x < view.origin.x + view.size.width &&
         coord.y >= view.origin.y && coord.y < view.origin.y + view.size.height;
}

enum class Colour : uint8_t
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

constexpr Character character(const uint16_t ch, const Modifier modifiers = Modifier::None,
                              const uint8_t colour_pair = 0)
{
  return (static_cast<Character>(ch) & 0x0000FFFFu) |
         (static_cast<Character>(static_cast<uint8_t>(modifiers)) << 16u) |
         (static_cast<Character>(static_cast<uint8_t>(colour_pair) & 0xffu) << 24u);
}

constexpr Character character(char ch, Modifier modifiers = Modifier::None, uint8_t colour_pair = 0)
{
  return character(static_cast<uint16_t>(ch), modifiers, colour_pair);
}

constexpr Character character(uint16_t ch, uint8_t colour_pair)
{
  return character(ch, Modifier::None, colour_pair);
}

constexpr Character character(char ch, uint8_t colour_pair)
{
  return character(static_cast<uint16_t>(ch), Modifier::None, colour_pair);
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

struct ScreenImp;
class WEAVER_EXPORT Screen
{
public:
  Screen();
  ~Screen();

  [[nodiscard]] Size size() const { return viewport().size; }

  bool setViewport(const Viewport &viewport);
  [[nodiscard]] Viewport viewport() const;

  [[nodiscard]] uint8_t defineColour(Colour fg, Colour bg);

  View &addLayer(int32_t id, const Viewport &viewport);
  View &layer(int32_t id);

  void draw();

  /// Clear the screen buffer. Not effected until the next @c draw() call.
  void clear();

  // Internal

  [[nodiscard]] ScreenImp &imp() { return *_imp; }
  [[nodiscard]] const ScreenImp &imp() const { return *_imp; }

private:
  std::unique_ptr<ScreenImp> _imp;
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
