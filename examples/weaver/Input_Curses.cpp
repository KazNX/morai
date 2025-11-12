#include "Input.hpp"

#include "Weaver.hpp"
#include "private/WeaverImp.hpp"

#include <curses.h>

#include <array>
#include <type_traits>
#include <utility>

namespace weaver
{
namespace
{
using KeyMapping = std::pair<int, Key>;
// Special keys:
// PageUp, 339
// PageDown, 338
// End, 260
// Home, 262
// ArrowLeft, 260
// ArrowUp, 259
// ArrowRight, 261
// ArrowDown, 258
// Print,
// Insert, 331
// Delete 330

// LeftShift,
// RightShift,
// LeftCtrl,
// RightCtrl,
// LeftAlt,
// RightAlt,
// LeftMeta,
// RightMeta,
// CapsLock,

// NumLock,
// Numpad0,
// Numpad1,
// Numpad2,
// Numpad3,
// Numpad4,
// Numpad5,
// Numpad6,
// Numpad7,
// Numpad8,
// Numpad9,
// NumpadDivide,
// NumpadMultiply,
// NumpadSubtract,
// NumpadAdd,
// NumpadDecimal,

// F1, 265
// F2, 266
// F3, 267
// F4, 268
// F5,
// F6,
// F7,
// F8,
// F9,
// F10,
// F11,
// F12, 276
// F13,
// F14,
// F15,
// F16,
// F17,
// F18,
// F19,
// F20,
// F21,
// F22,
// F23,
// F24,

constexpr std::array CHARACTER_MAP = {
  // KeyMapping{ 0, Key::None },
  KeyMapping{ 0x08, Key::Backspace },
  KeyMapping{ 0x09, Key::Tab },
  KeyMapping{ 0x10, Key::Enter },
  KeyMapping{ 0x1b, Key::Escape },
  KeyMapping{ ' ', Key::Space },
  KeyMapping{ ',', Key::Comma },
  KeyMapping{ '-', Key::Minus },
  KeyMapping{ '.', Key::Period },
  KeyMapping{ '/', Key::Slash },
  KeyMapping{ '0', Key::Num0 },
  KeyMapping{ '1', Key::Num1 },
  KeyMapping{ '2', Key::Num2 },
  KeyMapping{ '3', Key::Num3 },
  KeyMapping{ '4', Key::Num4 },
  KeyMapping{ '5', Key::Num5 },
  KeyMapping{ '6', Key::Num6 },
  KeyMapping{ '7', Key::Num7 },
  KeyMapping{ '8', Key::Num8 },
  KeyMapping{ '9', Key::Num9 },
  KeyMapping{ ';', Key::Semicolon },
  KeyMapping{ '=', Key::Equal },
  KeyMapping{ 'a', Key::A },
  KeyMapping{ 'b', Key::B },
  KeyMapping{ 'c', Key::C },
  KeyMapping{ 'd', Key::D },
  KeyMapping{ 'e', Key::E },
  KeyMapping{ 'f', Key::F },
  KeyMapping{ 'g', Key::G },
  KeyMapping{ 'h', Key::H },
  KeyMapping{ 'i', Key::I },
  KeyMapping{ 'j', Key::J },
  KeyMapping{ 'k', Key::K },
  KeyMapping{ 'l', Key::L },
  KeyMapping{ 'm', Key::M },
  KeyMapping{ 'n', Key::N },
  KeyMapping{ 'o', Key::O },
  KeyMapping{ 'p', Key::P },
  KeyMapping{ 'q', Key::Q },
  KeyMapping{ 'r', Key::R },
  KeyMapping{ 's', Key::S },
  KeyMapping{ 't', Key::T },
  KeyMapping{ 'u', Key::U },
  KeyMapping{ 'v', Key::V },
  KeyMapping{ 'w', Key::W },
  KeyMapping{ 'x', Key::X },
  KeyMapping{ 'y', Key::Y },
  KeyMapping{ 'z', Key::Z },
  KeyMapping{ 'A', Key::A },
  KeyMapping{ 'B', Key::B },
  KeyMapping{ 'C', Key::C },
  KeyMapping{ 'D', Key::D },
  KeyMapping{ 'E', Key::E },
  KeyMapping{ 'F', Key::F },
  KeyMapping{ 'G', Key::G },
  KeyMapping{ 'H', Key::H },
  KeyMapping{ 'I', Key::I },
  KeyMapping{ 'J', Key::J },
  KeyMapping{ 'K', Key::K },
  KeyMapping{ 'L', Key::L },
  KeyMapping{ 'M', Key::M },
  KeyMapping{ 'N', Key::N },
  KeyMapping{ 'O', Key::O },
  KeyMapping{ 'P', Key::P },
  KeyMapping{ 'Q', Key::Q },
  KeyMapping{ 'R', Key::R },
  KeyMapping{ 'S', Key::S },
  KeyMapping{ 'T', Key::T },
  KeyMapping{ 'U', Key::U },
  KeyMapping{ 'V', Key::V },
  KeyMapping{ 'W', Key::W },
  KeyMapping{ 'X', Key::X },
  KeyMapping{ 'Y', Key::Y },
  KeyMapping{ 'Z', Key::Z },
  KeyMapping{ '[', Key::LeftBracket },
  KeyMapping{ '\\', Key::Backslash },
  KeyMapping{ ']', Key::RightBracket },
  KeyMapping{ '`', Key::Grave },
  KeyMapping{ KEY_DC, Key::Delete },
  KeyMapping{ KEY_PREVIOUS, Key::PageUp },
  KeyMapping{ KEY_NEXT, Key::PageDown },
  KeyMapping{ KEY_END, Key::End },
  KeyMapping{ KEY_HOME, Key::Home },
  KeyMapping{ KEY_LEFT, Key::ArrowLeft },
  KeyMapping{ KEY_UP, Key::ArrowUp },
  KeyMapping{ KEY_RIGHT, Key::ArrowRight },
  KeyMapping{ KEY_DOWN, Key::ArrowDown },
  KeyMapping{ KEY_PRINT, Key::Print },
  KeyMapping{ KEY_IC, Key::Insert },
  // KeyMapping{ KEY_SHIFT, Key::LeftShift },
  // KeyMapping{ KEY_SHIFT, Key::RightShift },
  // KeyMapping{ KEY_CONTROL, Key::LeftCtrl },
  // KeyMapping{ KEY_CONTROL, Key::RightCtrl },
  // KeyMapping{ KEY_ALT, Key::LeftAlt },
  // KeyMapping{ KEY_ALT, Key::RightAlt },
  // KeyMapping{ KEY_META, Key::LeftMeta },
  // KeyMapping{ KEY_META, Key::RightMeta },
  // KeyMapping{ KEY_CAPS, Key::CapsLock },
  // KeyMapping{ KEY_NUM, Key::NumLock },
  // KeyMapping{ KEY_B2, Key::Numpad0 },
  // KeyMapping{ KEY_B2, Key::Numpad1 },
  // KeyMapping{ KEY_B2, Key::Numpad2 },
  // KeyMapping{ KEY_B2, Key::Numpad3 },
  // KeyMapping{ KEY_B2, Key::Numpad4 },
  // KeyMapping{ KEY_B2, Key::Numpad5 },
  // KeyMapping{ KEY_B2, Key::Numpad6 },
  // KeyMapping{ KEY_B2, Key::Numpad7 },
  // KeyMapping{ KEY_B2, Key::Numpad8 },
  // KeyMapping{ KEY_B2, Key::Numpad9 },
  // KeyMapping{ KEY_B2, Key::NumpadDivide },
  // KeyMapping{ KEY_B2, Key::NumpadMultiply },
  // KeyMapping{ KEY_B2, Key::NumpadSubtract },
  // KeyMapping{ KEY_B2, Key::NumpadAdd },
  // KeyMapping{ KEY_B2, Key::NumpadDecimal },
  KeyMapping{ KEY_F0 + 1, Key::F1 },
  KeyMapping{ KEY_F0 + 2, Key::F2 },
  KeyMapping{ KEY_F0 + 3, Key::F3 },
  KeyMapping{ KEY_F0 + 4, Key::F4 },
  KeyMapping{ KEY_F0 + 5, Key::F5 },
  KeyMapping{ KEY_F0 + 6, Key::F6 },
  KeyMapping{ KEY_F0 + 7, Key::F7 },
  KeyMapping{ KEY_F0 + 8, Key::F8 },
  KeyMapping{ KEY_F0 + 9, Key::F9 },
  KeyMapping{ KEY_F0 + 10, Key::F10 },
  KeyMapping{ KEY_F0 + 11, Key::F11 },
  KeyMapping{ KEY_F0 + 12, Key::F12 },
};

[[nodiscard]] Key mapInput(const int ch)
{
  for (const auto &mapping : CHARACTER_MAP)
  {
    if (mapping.first == ch)
    {
      return mapping.second;
    }
  }
  return Key::None;
}
}  // namespace

struct Input::Imp
{
  WINDOW *window = nullptr;
  std::array<bool, KEY_COUNT> keys{};

  void onInput(int ch)
  {
    const auto key = mapInput(ch);
    std::ranges::fill(keys, false);
    if (key != Key::None)
    {
      keys[static_cast<std::underlying_type_t<Key>>(key)] = true;
    }
  }
};

Input::Input(Screen &screen)
  : _imp(std::make_unique<Imp>())
{
  _imp->window = screen.imp().window.get();
  screen.imp().input_hook = [this](const int ch) { _imp->onInput(ch); };
}

Input::~Input() = default;

void Input::poll()
{}

KeyState Input::keyState(const Key key)
{
  return _imp->keys[static_cast<std::underlying_type_t<Key>>(key)] ? KeyState::Down : KeyState::Up;
}

bool Input::anyKeyDown(std::span<const Key> keys)
{
  for (const auto &key : keys)
  {
    if (keyState(key) == KeyState::Down)
    {
      return true;
    }
  }
  return false;
}

bool Input::anyKeyUp(std::span<const Key> keys)
{
  for (const auto &key : keys)
  {
    if (keyState(key) == KeyState::Up)
    {
      return true;
    }
  }
  return false;
}
}  // namespace weaver
