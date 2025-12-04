#include "Input.hpp"

#include <array>
#include <utility>

#include <windows.h>

namespace weaver
{
namespace
{
using KeyMapping = std::pair<uint32_t, Key>;
constexpr std::array CHARACTER_MAP = {
  // KeyMapping{ 0x0000u, Key::None },
  KeyMapping{ VK_BACK, Key::Backspace },
  KeyMapping{ VK_TAB, Key::Tab },
  KeyMapping{ VK_RETURN, Key::Enter },
  KeyMapping{ VK_ESCAPE, Key::Escape },
  KeyMapping{ VK_SPACE, Key::Space },
  KeyMapping{ VK_OEM_COMMA, Key::Comma },
  KeyMapping{ VK_OEM_MINUS, Key::Minus },
  KeyMapping{ VK_OEM_PERIOD, Key::Period },
  KeyMapping{ VK_OEM_2, Key::Slash },
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
  KeyMapping{ VK_OEM_1, Key::Semicolon },
  KeyMapping{ VK_OEM_PLUS, Key::Equal },
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
  KeyMapping{ VK_OEM_4, Key::LeftBracket },
  KeyMapping{ VK_OEM_5, Key::Backslash },
  KeyMapping{ VK_OEM_6, Key::RightBracket },
  KeyMapping{ VK_OEM_3, Key::Grave },
  KeyMapping{ VK_DELETE, Key::Delete },
  KeyMapping{ VK_PRIOR, Key::PageUp },
  KeyMapping{ VK_NEXT, Key::PageDown },
  KeyMapping{ VK_END, Key::End },
  KeyMapping{ VK_HOME, Key::Home },
  KeyMapping{ VK_LEFT, Key::ArrowLeft },
  KeyMapping{ VK_UP, Key::ArrowUp },
  KeyMapping{ VK_RIGHT, Key::ArrowRight },
  KeyMapping{ VK_DOWN, Key::ArrowDown },
  KeyMapping{ VK_PRINT, Key::Print },
  KeyMapping{ VK_INSERT, Key::Insert },
  KeyMapping{ VK_LSHIFT, Key::LeftShift },
  KeyMapping{ VK_RSHIFT, Key::RightShift },
  KeyMapping{ VK_LCONTROL, Key::LeftCtrl },
  KeyMapping{ VK_RCONTROL, Key::RightCtrl },
  KeyMapping{ VK_LMENU, Key::LeftAlt },
  KeyMapping{ VK_RMENU, Key::RightAlt },
  KeyMapping{ VK_LWIN, Key::LeftMeta },
  KeyMapping{ VK_RWIN, Key::RightMeta },
  KeyMapping{ VK_CAPITAL, Key::CapsLock },
  KeyMapping{ VK_NUMLOCK, Key::NumLock },
  KeyMapping{ VK_NUMPAD0, Key::Numpad0 },
  KeyMapping{ VK_NUMPAD1, Key::Numpad1 },
  KeyMapping{ VK_NUMPAD2, Key::Numpad2 },
  KeyMapping{ VK_NUMPAD3, Key::Numpad3 },
  KeyMapping{ VK_NUMPAD4, Key::Numpad4 },
  KeyMapping{ VK_NUMPAD5, Key::Numpad5 },
  KeyMapping{ VK_NUMPAD6, Key::Numpad6 },
  KeyMapping{ VK_NUMPAD7, Key::Numpad7 },
  KeyMapping{ VK_NUMPAD8, Key::Numpad8 },
  KeyMapping{ VK_NUMPAD9, Key::Numpad9 },
  KeyMapping{ VK_DIVIDE, Key::NumpadDivide },
  KeyMapping{ VK_MULTIPLY, Key::NumpadMultiply },
  KeyMapping{ VK_SUBTRACT, Key::NumpadSubtract },
  KeyMapping{ VK_ADD, Key::NumpadAdd },
  KeyMapping{ VK_DECIMAL, Key::NumpadDecimal },
  KeyMapping{ VK_F1, Key::F1 },
  KeyMapping{ VK_F2, Key::F2 },
  KeyMapping{ VK_F3, Key::F3 },
  KeyMapping{ VK_F4, Key::F4 },
  KeyMapping{ VK_F5, Key::F5 },
  KeyMapping{ VK_F6, Key::F6 },
  KeyMapping{ VK_F7, Key::F7 },
  KeyMapping{ VK_F8, Key::F8 },
  KeyMapping{ VK_F9, Key::F9 },
  KeyMapping{ VK_F1, Key::F10 },
  KeyMapping{ VK_F1, Key::F11 },
  KeyMapping{ VK_F1, Key::F12 },
  KeyMapping{ VK_F1, Key::F13 },
  KeyMapping{ VK_F1, Key::F14 },
  KeyMapping{ VK_F1, Key::F15 },
  KeyMapping{ VK_F1, Key::F16 },
  KeyMapping{ VK_F1, Key::F17 },
  KeyMapping{ VK_F1, Key::F18 },
  KeyMapping{ VK_F1, Key::F19 },
  KeyMapping{ VK_F2, Key::F20 },
  KeyMapping{ VK_F2, Key::F21 },
  KeyMapping{ VK_F2, Key::F22 },
  KeyMapping{ VK_F2, Key::F23 },
  KeyMapping{ VK_F2, Key::F24 },
};

[[nodiscard]] uint32_t mapKey(const Key key)
{
  for (const auto &mapping : CHARACTER_MAP)
  {
    if (mapping.second == key)
    {
      return mapping.first;
    }
  }
  return 0u;
}
}  // namespace

struct Input::Imp
{
  // Nothing required.
};

Input::Input([[maybe_unused]] Screen &screen)
  : _imp{ std::make_unique<Imp>() }
{}

Input::~Input() = default;

void Input::poll()
{}

KeyState Input::keyState(const Key key)
{
  const uint32_t vk_code = mapKey(key);

  if (key == Key::Space)
  {
    // Use GetKeyState for VK_SPACE as a workaround
    const SHORT state = GetKeyState(VK_SPACE);
    if ((state & 0x8000) != 0)
    {
      return KeyState::Down;
    }
    return KeyState::Up;
  }
  if (vk_code)
  {
    const SHORT state = GetAsyncKeyState(static_cast<int>(vk_code));
    if ((state & 0x8000) != 0)
    {
      return KeyState::Down;
    }
  }
  return KeyState::Up;
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
