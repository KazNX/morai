#include <arachne/Finally.hpp>
#include <arachne/Move.hpp>
#include <arachne/Scheduler.hpp>

#include <gtest/gtest.h>

#include <array>

namespace arachne
{
TEST(Move, scheduler)
{
  // Test moving fibres between schedulers.
  struct State
  {
    std::array<Scheduler, 2> schedulers;
    int running_on = 0;
  } state;

  const auto mover = [](State &state, int scheduler_idx) -> Fibre {
    state.running_on = scheduler_idx;
    for (;;)
    {
      scheduler_idx = 1 - scheduler_idx;
      state.running_on = scheduler_idx;
      co_await moveTo(state.schedulers[scheduler_idx]);
    }
  };

  state.schedulers[0].start(mover(state, 0));
  EXPECT_EQ(state.running_on, 0);
  EXPECT_EQ(state.schedulers[0].runningCount(), 1);
  EXPECT_EQ(state.schedulers[1].runningCount(), 0);

  double elapsed = 0.0;
  const double dt = 0.1;

  // Every odd update should see the fibres on the scheduler matching their own ID.
  // First update sees fibre enter up to the move request, so moves won't be completed yet.
  for (int i = 0; i < 100; ++i, elapsed += dt)
  {
    // Update the scheduler it's on right now. That will move the thread once we
    // update other.
    const int initially_on = state.running_on;
    EXPECT_EQ(state.schedulers[initially_on].runningCount(), 1);
    EXPECT_EQ(state.schedulers[1 - initially_on].runningCount(), 0);
    state.schedulers[initially_on].update(elapsed);

    // Will have changed schedulers on the first update.
    EXPECT_NE(initially_on, state.running_on);
    EXPECT_EQ(state.schedulers[state.running_on].runningCount(), 1);
    EXPECT_EQ(state.schedulers[1 - state.running_on].runningCount(), 0);

    // Will change back now.
    state.schedulers[1 - initially_on].update(elapsed);
  }
}
}  // namespace arachne
