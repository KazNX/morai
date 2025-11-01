#pragma once

#include <cstddef>
#include <functional>

namespace arachne
{
using Id = std::size_t;
using WaitCondition = std::function<bool()>;
constexpr Id InvalidFibre = ~static_cast<std::size_t>(0);
}  // namespace arachne
