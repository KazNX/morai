import arachne;

import std;

#include <gtest/gtest.h>

namespace arachne
{
Fibre ticker()
{
  for (int i = 0; i < 5; ++i)
  {
    std::cout << "Tick " << i << '\n';
    co_yield {};
  }
  std::cout << "Tick done\n";
}

TEST(Fibre, ticker)
{
  Scheduler scheduler;
  Id fibre_id = scheduler.start(ticker());

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(scheduler.isRunning(fibre_id));
  while (scheduler.isRunning(fibre_id))
  {
    scheduler.update(simulated_time_s);
    simulated_time_s += dt;
  }

  EXPECT_FALSE(scheduler.isRunning(fibre_id));
}

Fibre cancellation(bool *cleaned_up)
{
  [[maybe_unused]] const auto at_exit = arachne::finally([&cleaned_up]() {
    if (cleaned_up)
    {
      *cleaned_up = true;
    }
  });

  for (int i = 0;; ++i)
  {
    std::cout << "Tick " << i << '\n';
    co_yield {};
  }
  std::cout << "Tick done\n";
}

TEST(Fibre, cancellation)
{
  Scheduler scheduler;
  bool cleaned_up = false;
  Id fibre_id = scheduler.start(cancellation(&cleaned_up));

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(scheduler.isRunning(fibre_id));
  for (int i = 0; i < 5; ++i)
  {
    scheduler.update(simulated_time_s);
    simulated_time_s += dt;
  }

  scheduler.cancel(fibre_id);
  EXPECT_FALSE(scheduler.isRunning(fibre_id));
}


TEST(Fibre, await)
{
  struct SharedState
  {
    double time = 0;
    double waiter_end_time = 0;
    double signaller_end_time = 0;
    bool signal = false;
  };

  Scheduler scheduler;
  SharedState state;

  const auto waiter_fibre = [&state]() -> Fibre {
    std::cout << "Waiter fibre started\n";
    co_await [&state]() { return state.signal; };
    // Yield one more loop;
    co_yield {};
    std::cout << "Waiter fibre done\n";
    state.waiter_end_time = state.time;
  };

  const auto signaller_fibre = [&state]() -> Fibre {
    std::cout << "Signaller fibre started\n";
    co_await std::chrono::milliseconds(10);
    state.signal = true;
    std::cout << "Signaller fibre done\n";
    state.waiter_end_time = state.time;
  };

  scheduler.start(waiter_fibre());
  scheduler.start(signaller_fibre());

  const auto update_dt = 0.01;
  for (state.time = 0; state.time < 1.0 && !scheduler.empty(); state.time += update_dt)
  {
    scheduler.update(state.time);
  }

  EXPECT_TRUE(state.signal);
  EXPECT_TRUE(scheduler.empty());
  EXPECT_LE(state.signaller_end_time, state.waiter_end_time);
}

TEST(Fibre, spawn)
{
  Scheduler scheduler;

  auto child_fibre = [](int id) -> Fibre {
    std::cout << "Child fibre " << id << " started\n";
    co_yield {};
    std::cout << "Child fibre " << id << " done\n";
  };

  auto parent_fibre = [&scheduler, &child_fibre]() -> Fibre {
    std::cout << "Parent fibre started\n";
    Id child1 = scheduler.start(child_fibre(1));
    Id child2 = scheduler.start(child_fibre(2));

    co_await scheduler.await(child1);
    co_await scheduler.await(child2);

    EXPECT_FALSE(scheduler.isRunning(child1));
    EXPECT_FALSE(scheduler.isRunning(child2));

    std::cout << "Parent fibre done\n";
  };

  Id parent_id = scheduler.start(parent_fibre());

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(scheduler.isRunning(parent_id));
  while (scheduler.isRunning(parent_id))
  {
    scheduler.update(simulated_time_s);
    simulated_time_s += dt;
  }

  EXPECT_FALSE(scheduler.isRunning(parent_id));
  EXPECT_TRUE(scheduler.empty());
}


TEST(Fibre, spawnAndCancel)
{
  Scheduler scheduler;

  auto child_fibre = [](int id, bool persist = false) -> Fibre {
    std::cout << std::format("Child fibre {} started - persist {}\n", id, persist);
    if (persist)
    {
      for (;;)
      {
        co_yield {};
      }
    }
    else
    {
      co_yield {};
    }
    std::cout << "Child fibre " << id << " done\n";
  };

  auto parent_fibre = [&scheduler, &child_fibre]() -> Fibre {
    std::cout << "Parent fibre started\n";
    Id child1 = scheduler.start(child_fibre(1, true));
    Id child2 = scheduler.start(child_fibre(2, false));

    co_await scheduler.await(child2);

    EXPECT_TRUE(scheduler.isRunning(child1));
    EXPECT_FALSE(scheduler.isRunning(child2));
    scheduler.cancel(child1);

    std::cout << "Parent fibre done\n";
  };

  Id parent_id = scheduler.start(parent_fibre());
  Id persistent_id = scheduler.start(child_fibre(99, true));

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(scheduler.isRunning(parent_id));
  while (scheduler.isRunning(parent_id))
  {
    scheduler.update(simulated_time_s);
    simulated_time_s += dt;
  }

  EXPECT_FALSE(scheduler.isRunning(parent_id));
  EXPECT_TRUE(scheduler.isRunning(persistent_id));
  scheduler.cancelAll();
  EXPECT_TRUE(scheduler.empty());
}
}  // namespace arachne
