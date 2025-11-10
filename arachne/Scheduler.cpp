#include "Finally.hpp"
#include "Scheduler.hpp"

namespace arachne
{
Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

bool Scheduler::isRunning(const Id fibre_id) const noexcept
{
  return std::ranges::find_if(
           _fibres, [fibre_id](const FibreEntry &state) { return state.id == fibre_id; }) !=
           _fibres.end() ||
         std::ranges::find_if(_new_fibres, [fibre_id](const FibreEntry &state) {
           return state.id == fibre_id;
         }) != _new_fibres.end();
}

Id Scheduler::start(Fibre &&fibre)
{
  const Id fibre_id = _next_id;
  // Increment by 2 so overflow skips InvalidFibre. Unlikely, but you never know
  _next_id += 2;
  auto &fibre_set = (_in_update) ? _new_fibres : _fibres;
  fibre_set.emplace_back(FibreEntry{ .id = fibre_id, .fibre = std::move(fibre) });
  return fibre_id;
}

bool Scheduler::cancel(const Id fibre_id)
{
  if (cancel(_fibres, _expiry, fibre_id) || cancel(_new_fibres, _expiry, fibre_id))
  {
    if (!_in_update)
    {
      cleanupFibres(_expiry);
    }
    return true;
  }
  return false;
}

std::size_t Scheduler::cancel(std::span<const Id> fibre_ids)
{
  std::size_t removed = cancel(_fibres, _expiry, fibre_ids);
  if (_expiry.indices.size() < fibre_ids.size())
  {
    removed += cancel(_new_fibres, _expiry, fibre_ids);
  }

  if (!_in_update)
  {
    cleanupFibres(_expiry);
  }

  return removed;
}

void Scheduler::cancelAll()
{
  _fibres.clear();
}

void Scheduler::update(const double epoch_time_s)
{
  // Mark update and setup to clear on leaving scope.
  _in_update = true;
  [[maybe_unused]] const auto cleanup = finally([this]() { _in_update = false; });

  _time.dt = epoch_time_s - _time.epoch_time_s;
  _time.epoch_time_s = epoch_time_s;
  _expiry.clear();
  for (std::size_t idx = 0; auto &&fibre : _fibres)
  {
    // Check for resumption
    if (fibre.cancel || fibre.fibre.resume(epoch_time_s) == Resume::Expire) [[unlikely]]
    {
      _expiry.indices.push_back(idx);
    }
    ++idx;
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

bool Scheduler::cancel(std::vector<FibreEntry> &fibres, Expiry &expiry, const Id fibre_id) const
{
  for (std::size_t i = 0; i < fibres.size(); ++i)
  {
    auto &fibre = fibres.at(i);
    if (fibre.id == fibre_id)
    {
      fibre.cancel = true;
      expiry.indices.emplace_back(i);
      return true;
    }
  }
  return false;
}

std::size_t Scheduler::cancel(std::vector<FibreEntry> &fibres, Expiry &expiry,
                              std::span<const Id> fibre_ids) const
{
  std::size_t removed = 0;
  for (std::size_t i = 0; i < fibres.size(); ++i)
  {
    auto &fibre = fibres.at(i);
    if (std::ranges::find(fibre_ids, fibre.id) != fibre_ids.end())
    {
      fibre.cancel = true;
      expiry.indices.emplace_back(i);
      ++removed;
    }
  }

  return removed;
}

void Scheduler::cleanupFibres(Expiry &expiry)
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

}  // namespace arachne
