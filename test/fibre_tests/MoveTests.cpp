#include <arachne/Finally.hpp>
#include <arachne/Move.hpp>
#include <arachne/Scheduler.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <ranges>
#include <random>

namespace arachne
{
TEST(Move, Scheduler)
{
  // Test moving fibres between schedulers.
  // struct SharedData
  // {
  //   std::array<Scheduler, 2> schedulers;
  // } shared;

  // const auto mover = [](SharedData &shared, int schduler_idx) -> Fibre {
  //   for (;;)
  //   {
  //     schduler_idx = 1 - schduler_idx;
  //     co_await moveTo(shared.schedulers[schduler_idx]);
  //   }
  // };

  // std::array<Id, 2> fibres = {
  //   shared.schedulers[0].start(mover(shared, 0)),
  //   shared.schedulers[1].start(mover(shared, 1)),
  // };
}  // namespace arachne
}  // namespace arachne
