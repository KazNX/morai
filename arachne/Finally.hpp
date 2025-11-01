#pragma once

#include <utility>

namespace arachne
{
/// FinalAction allows you to ensure something gets run at the end of a scope
///
/// This comes from Microsoft GLS library -
/// https://github.com/microsoft/GSL/blob/main/include/gsl/util
///
/// Prefer using @c finally().
template <class F>
class FinalAction
{
public:
  explicit FinalAction(const F &action) noexcept
    : _action{ action }
  {}
  explicit FinalAction(F &&action) noexcept
    : _action{ std::move(action) }
  {}

  ~FinalAction() noexcept
  {
    if (_invoke)
    {
      _action();
    }
  }

  FinalAction(FinalAction &&other) noexcept
    : _action(std::move(other._action))
    , _invoke(std::exchange(other._invoke, false))
  {}

  FinalAction(const FinalAction &) = delete;
  void operator=(const FinalAction &) = delete;
  void operator=(FinalAction &&) = delete;

private:
  F _action{};
  bool _invoke = true;
};

/// finally() - convenience function to generate a FinalAction. Accepts any
/// callable object to create a @c FinalAction. The callable is invoked when
/// the @c FinalAction goes out of scope.
///
/// Example usage:
///
/// @code
/// void someFunction()
/// {
///   [[maybe_unused]] const auto on_exit = arachne::finally([]() {
///     std::cout << "Leaving someFunction()\n";
///   });
///   // Function code here.
/// }
/// @endcode
///
/// This comes from Microsoft GLS library -
/// https://github.com/microsoft/GSL/blob/main/include/gsl/util
template <class F>
  requires std::is_invocable_v<F>
[[nodiscard]] auto finally(F &&action) noexcept
{
  return FinalAction<std::decay_t<F>>{ std::forward<F>(action) };
}
}  // namespace arachne
