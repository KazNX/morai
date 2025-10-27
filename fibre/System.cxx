module fibre;

namespace fibre
{
namespace
{
struct OnExit
{
  std::function<void()> func;

  OnExit(std::function<void()> func)
    : func(std::move(func))
  {}

  ~OnExit()
  {
    if (func)
    {
      func();
    }
  }
};
}  // namespace

System::System() = default;
System::~System() = default;

bool System::isRunning(const Id fibre_id) const noexcept
{
  return std::ranges::find_if(
           _fibres, [fibre_id](const FibreEntry &state) { return state.id == fibre_id; }) !=
           _fibres.end() ||
         std::ranges::find_if(_new_fibres, [fibre_id](const FibreEntry &state) {
           return state.id == fibre_id;
         }) != _new_fibres.end();
}

Id System::start(Fibre &&fibre)
{
  const Id fibre_id = _next_id;
  // Increment by 2 so overflow skips InvalidFibre. Unlikely, but you never know
  _next_id += 2;
  auto &fibre_set = (_in_update) ? _new_fibres : _fibres;
  fibre_set.emplace_back(FibreEntry{ .id = fibre_id, .fibre = std::move(fibre) });
  return fibre_id;
}

bool System::cancel(const Id fibre_id)
{
  return cancel(_fibres, fibre_id, _in_update) || cancel(_new_fibres, fibre_id, false);
}

std::size_t System::cancel(std::span<const Id> fibre_ids)
{
  _expiry.clear();
  std::size_t removed = cancel(_fibres, fibre_ids, _in_update, _expiry);
  if (removed == fibre_ids.size())
  {
    return removed;
  }

  removed += cancel(_new_fibres, fibre_ids, false, _expiry);
  return removed;
  {
    cleanupFibres(_expiry);
  }
}

void System::cancelAll()
{
  _fibres.clear();
}

void System::update(const double epoch_time_s)
{
  // Mark update and setup to clear on leaving scope.
  _in_update = true;
  OnExit on_exit([this]() { _in_update = false; });

  _expiry.clear();
  for (auto &&fibre : _fibres)
  {
    // Check for resumption
    if (fibre.cancel || fibre.fibre.resume(epoch_time_s) == Resume::Expire) [[unlikely]]
    {
      _expiry.indices.push_back(*_current_update_idx);
    }
  }

  cleanupFibres(_expiry);

  // Migrate new fibres to the main set.
  if (!_new_fibres.empty())
  {
    _fibres.insert(_fibres.end(), std::make_move_iterator(_new_fibres.begin()),
                   std::make_move_iterator(_new_fibres.end()));
    _new_fibres.clear();
  }
}

bool System::cancel(std::vector<FibreEntry> &fibres, const Id fibre_id,
                    const bool defer_cleanup) const
{
  for (std::size_t i = 0; i < _fibres.size(); ++i)
  {
    auto &fibre = _fibres.at(i);
    if (fibre.id == fibre_id)
    {
      fibre.cancel = true;
      if (!defer_cleanup)
      {
        handleCancellation(i, fibre);
      }
      else
      {
        _expiry.indices.emplace_back(i);
      }
      return true;
    }
  }
  return false;
}

std::size_t System::cancel(std::vector<FibreEntry> &fibres, std::span<const Id> fibre_ids,
                           bool defer_cleanup, Expiry &expiry) const
{
  std::size_t removed = 0;
  for (std::size_t i = 0; i < _fibres.size(); ++i)
  {
    auto &fibre = _fibres.at(i);
    if (std::ranges::find(fibre_ids, fibre.id) != fibre_ids.end())
    {
      fibre.cancel = true;
      expiry.indices.emplace_back(i);
      ++removed;
    }
  }

  if (!defer_cleanup)
  {
    cleanupFibres(expiry);
  }
  return removed;
}

void System::handleCancellation(const std::size_t idx, FibreEntry &fibre)
{
  // Mark for cancellation. This is enough if we haven't updated this
  // fibre in an ongoing update().
  fibre.cancel = true;
  // If not in an update or if the cancelled fibre has already been updated
  // this cycle.
  if (!_current_update_idx || idx < *_current_update_idx)
  {
    // Add to expiry for later cleanup.
    _expiry.indices.emplace_back(idx);
    if (!_current_update_idx)
    {
      // Cleanup if not currently updating.
      cleanupFibres(_expiry);
    }
  }
}

void System::cleanupFibres(Expiry &expiry)
{
  if (expiry.indices.empty())
  {
    return;
  }

  auto remove_iter = expiry.indices.begin();
  for (std::size_t i = 0; i < _fibres.size(); ++i)
  {
    if (remove_iter != expiry.indices.end() && i == *remove_iter)
    {
      ++remove_iter;
    }
    else
    {
      expiry.fibres.emplace_back(std::move(_fibres.at(i)));
    }
  }

  std::swap(_fibres, expiry.fibres);
  expiry.clear();
}

}  // namespace fibre
