module fibre;

namespace fibre
{
System::System() = default;
System::~System() = default;

bool System::isRunning(const Id fibre_id) const noexcept
{
  return std::ranges::find_if(_fibres, [fibre_id](const FibreState &state) {
           return state.id == fibre_id;
         }) != _fibres.end();
}

Id System::start(Fibre &&fibre)
{
  const Id fibre_id = _next_id++;
  _fibres.emplace_back(FibreState{ .id = fibre_id, .fibre = std::move(fibre), .resume = Yield{} });
  return fibre_id;
}

bool System::cancel(const Id fibre_id)
{
  for (std::size_t i = 0; i < _fibres.size(); ++i)
  {
    auto &fibre = _fibres.at(i);
    if (fibre.id == fibre_id)
    {
      _expiry.indices.emplace_back(i);
      cleanupFibres(_expiry);
      return true;
    }
  }
  return false;
}

std::size_t System::cancel(std::span<const Id> fibre_ids)
{
  std::size_t removed = 0;
  for (std::size_t i = 0; i < _fibres.size(); ++i)
  {
    auto &fibre = _fibres.at(i);
    if (std::ranges::find(fibre_ids, fibre.id) != fibre_ids.end())
    {
      _expiry.indices.emplace_back(i);
      ++removed;
    }
  }

  cleanupFibres(_expiry);
  return removed;
}

void System::cancelAll()
{
  _fibres.clear();
}

void System::update(const double epoch_time_s)
{
  _context.dt = epoch_time_s - _context.epoch_time_s;
  _context.epoch_time_s = epoch_time_s;

  _expiry.clear();
  for (size_t idx = 0; auto &&fibre : _fibres)
  {
    // Check for resumption
    if (fibre.should_resume(epoch_time_s))
    {
      fibre.resume = fibre.fibre.resume();
      fibre.resume.time_s =
        (fibre.resume.time_s >= 0) ? epoch_time_s + fibre.resume.time_s : fibre.resume.time_s;
      if (fibre.fibre.done())
      {
        _expiry.indices.push_back(idx);
      }
    }
    ++idx;
  }

  cleanupFibres(_expiry);
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
