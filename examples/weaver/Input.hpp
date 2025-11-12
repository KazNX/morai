#pragma once

#include "WeaverExport.hpp"

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>

namespace weaver
{
enum class Key : uint16_t
{
  // ASCII key codes
  None = 0,
  Backspace = 8,
  Tab = 9,
  LineFeed = 10,
  Enter = 13,
  Escape = 27,
  Space = 32,
  Exclamation = 33,
  DoubleQuote = 34,
  Hash = 35,
  Dollar = 36,
  Percent = 37,
  Ampersand = 38,
  SingleQuote = 39,
  LeftParenthesis = 40,
  RightParenthesis = 41,
  Asterisk = 42,
  Plus = 43,
  Comma = 44,
  Minus = 45,
  Period = 46,
  Slash = 47,
  Num0 = 48,
  Num1 = 49,
  Num2 = 50,
  Num3 = 51,
  Num4 = 52,
  Num5 = 53,
  Num6 = 54,
  Num7 = 55,
  Num8 = 56,
  Num9 = 57,
  Semicolon = 59,
  LessThan = 60,
  Equal = 61,
  GreaterThan = 62,
  QuestionMark = 63,
  At = 64,
  A = 65,
  B = 66,
  C = 67,
  D = 68,
  E = 69,
  F = 70,
  G = 71,
  H = 72,
  I = 73,
  J = 74,
  K = 75,
  L = 76,
  M = 77,
  N = 78,
  O = 79,
  P = 80,
  Q = 81,
  R = 82,
  S = 83,
  T = 84,
  U = 85,
  V = 86,
  W = 87,
  X = 88,
  Y = 89,
  Z = 90,
  LeftBracket = 91,
  Backslash = 92,
  RightBracket = 93,
  Caret = 94,
  Underscore = 95,
  Grave = 96,
  Backtick = Grave,
  a = 97,
  b = 98,
  c = 99,
  d = 100,
  e = 101,
  f = 102,
  g = 103,
  h = 104,
  i = 105,
  j = 106,
  k = 107,
  l = 108,
  m = 109,
  n = 110,
  o = 111,
  p = 112,
  q = 113,
  r = 114,
  s = 115,
  t = 116,
  u = 117,
  v = 118,
  w = 119,
  x = 120,
  y = 121,
  z = 122,
  LeftBrace = 123,
  Pipe = 124,
  VerticalBar = Pipe,
  RightBrace = 125,
  Tilde = 126,
  Delete = 127,

  // Non-ASCII codes
  PageUp,
  PageDown,
  End,
  Home,
  ArrowLeft,
  ArrowUp,
  ArrowRight,
  ArrowDown,
  Print,
  Insert,

  LeftShift,
  RightShift,
  LeftCtrl,
  RightCtrl,
  LeftAlt,
  RightAlt,
  LeftMeta,
  RightMeta,
  CapsLock,

  NumLock,
  Numpad0,
  Numpad1,
  Numpad2,
  Numpad3,
  Numpad4,
  Numpad5,
  Numpad6,
  Numpad7,
  Numpad8,
  Numpad9,
  NumpadDivide,
  NumpadMultiply,
  NumpadSubtract,
  NumpadAdd,
  NumpadDecimal,

  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  F13,
  F14,
  F15,
  F16,
  F17,
  F18,
  F19,
  F20,
  F21,
  F22,
  F23,
  F24,
};

constexpr size_t KEY_COUNT = static_cast<size_t>(Key::F24) + 1;

enum class KeyState : uint8_t
{
  Up = 0,
  Down = 1,
};

class Screen;

class WEAVER_EXPORT Input
{
public:
  Input(Screen &screen);
  ~Input();

  Input(const Input &) = delete;
  Input(Input &&) = delete;
  Input &operator=(const Input &) = delete;
  Input &operator=(Input &&) = delete;

  void poll();

  [[nodiscard]] KeyState keyState(Key key);
  [[nodiscard]] bool anyKeyDown(std::span<const Key> keys);
  [[nodiscard]] bool anyKeyUp(std::span<const Key> keys);

  [[nodiscard]] inline bool anyKeyDown(std::initializer_list<Key> keys)
  {
    return anyKeyDown(std::span<const Key>{ keys.begin(), keys.size() });
  }

  [[nodiscard]] inline bool anyKeyUp(std::initializer_list<Key> keys)
  {
    return anyKeyUp(std::span<const Key>{ keys.begin(), keys.size() });
  }

private:
  struct Imp;

  std::unique_ptr<Imp> _imp;
};

}  // namespace weaver
