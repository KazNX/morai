#include <arachne/Finally.hpp>
#include <arachne/Scheduler.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <ranges>
#include <random>

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

TEST(Fibre, cancelUnknown)
{
  Scheduler scheduler;
  Id unknown_id{ 9999 };

  EXPECT_FALSE(unknown_id.isRunning());
  EXPECT_FALSE(scheduler.cancel(unknown_id));
}

TEST(Fibre, ticker)
{
  Scheduler scheduler;
  const Id fibre_id = scheduler.start(ticker(), "ticker");

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(fibre_id.isRunning());
  while (fibre_id.isRunning())
  {
    scheduler.update(simulated_time_s);
    simulated_time_s += dt;
  }

  EXPECT_FALSE(fibre_id.isRunning());
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
  const Id fibre_id = scheduler.start(cancellation(&cleaned_up), "cancellation");

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(fibre_id.isRunning());
  for (int i = 0; i < 5; ++i)
  {
    scheduler.update(simulated_time_s);
    simulated_time_s += dt;
  }

  scheduler.cancel(fibre_id);
  EXPECT_FALSE(fibre_id.isRunning());
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

  scheduler.start(waiter_fibre(), "waiter");
  scheduler.start(signaller_fibre(), "signaller");

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

  const auto child_fibre = [](int id) -> Fibre {
    std::cout << "Child fibre " << id << " started\n";
    co_yield {};
    std::cout << "Child fibre " << id << " done\n";
  };

  const auto parent_fibre = [&scheduler, &child_fibre]() -> Fibre {
    std::cout << "Parent fibre started\n";
    Id child1 = scheduler.start(child_fibre(1), "child1");
    Id child2 = scheduler.start(child_fibre(2), "child2");

    co_await child1;
    co_await child2;

    EXPECT_FALSE(child1.isRunning());
    EXPECT_FALSE(child2.isRunning());

    std::cout << "Parent fibre done\n";
  };

  Id parent_id = scheduler.start(parent_fibre(), "parent");

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(parent_id.isRunning());
  while (parent_id.isRunning())
  {
    scheduler.update(simulated_time_s);
    simulated_time_s += dt;
  }

  EXPECT_FALSE(parent_id.isRunning());
  EXPECT_TRUE(scheduler.empty());
}


TEST(Fibre, spawnAndCancel)
{
  Scheduler scheduler;

  const auto child_fibre = [](int id, bool persist = false) -> Fibre {
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

  const auto parent_fibre = [&scheduler, &child_fibre]() -> Fibre {
    std::cout << "Parent fibre started\n";
    Id child1 = scheduler.start(child_fibre(1, true), "child1");
    Id child2 = scheduler.start(child_fibre(2, false), "child2");

    co_await child2;

    EXPECT_TRUE(child1.isRunning());
    EXPECT_FALSE(child2.isRunning());
    scheduler.cancel(child1);

    std::cout << "Parent fibre done\n";
  };

  const Id parent_id = scheduler.start(parent_fibre(), "parent");
  const Id persistent_id = scheduler.start(child_fibre(99, true), "persistent");

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(parent_id.isRunning());
  while (parent_id.isRunning())
  {
    scheduler.update(simulated_time_s);
    simulated_time_s += dt;
  }

  EXPECT_FALSE(parent_id.isRunning());
  EXPECT_TRUE(persistent_id.isRunning());
  scheduler.cancelAll();
  EXPECT_TRUE(scheduler.empty());
}

TEST(Fibre, exceptionPropagation)
{
  Scheduler scheduler;

  const auto faulty_fibre = []() -> Fibre {
    std::cout << "Faulty fibre started\n";
    co_yield {};
    throw std::runtime_error("Something went wrong in the fibre");
    co_yield {};
  };

  const Id fibre_id = scheduler.start(faulty_fibre(), "faulty");

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(fibre_id.isRunning());
  bool exception_caught = false;
  while (fibre_id.isRunning())
  {
    try
    {
      scheduler.update(simulated_time_s);
    }
    catch (const std::runtime_error &e)
    {
      std::cout << "Caught exception from fibre: " << e.what() << '\n';
      exception_caught = true;
    }
    simulated_time_s += dt;
  }

  EXPECT_FALSE(fibre_id.isRunning());
  EXPECT_TRUE(exception_caught);
}

void priorityTest(std::span<const std::pair<int32_t, int32_t>> id_priority_pairs,
                  bool const log = false, const uint32_t queue_size = 1024u)
{
  SchedulerParams params;
  params.initial_queue_size = queue_size;
  for (const auto &pair : id_priority_pairs)
  {
    if (std::ranges::find(params.priority_levels, pair.second) == params.priority_levels.end())
    {
      params.priority_levels.emplace_back(pair.second);
    }
  }

  // Resolve the expected order.
  std::vector<int32_t> expected_order;
  for (const auto idx :
       id_priority_pairs | std::views::transform([](const auto &pair) { return pair.first; }))
  {
    expected_order.emplace_back(idx);
  }
  std::ranges::sort(expected_order);

  Scheduler scheduler{ std::move(params) };
  std::vector<int32_t> execution_order;
  std::vector<int32_t> shutdown_order;

  const auto fibre_entry = [&execution_order, &shutdown_order, log](const int32_t id) -> Fibre {
    if (log)
    {
      std::cout << "Fibre " << id << " started\n";
    }
    execution_order.emplace_back(id);
    co_yield {};
    if (log)
    {
      std::cout << "Fibre " << id << " done\n";
    }
    shutdown_order.emplace_back(id);
  };

  auto insert_validation = [&expected_order, &scheduler]() { return !expected_order.empty(); };

  for (const auto &id_priority_pair : id_priority_pairs)
  {
    scheduler.start(fibre_entry(id_priority_pair.first), id_priority_pair.second,
                    std::format("fibre{}", id_priority_pair.first));
  }

  scheduler.update(0);
  scheduler.update(1);

  EXPECT_EQ(execution_order, expected_order);
}

TEST(Fibre, priority)
{
  std::array priorities{ std::pair<int, int>{ 0, 300 }, std::pair<int, int>{ 1, 100 },
                         std::pair<int, int>{ 2, 400 }, std::pair<int, int>{ 3, -200 },
                         std::pair<int, int>{ 4, 0 },   std::pair<int, int>{ 5, 150 }

  };
  priorityTest(priorities, true);
}

TEST(Fibre, randomPriority)
{
  const int32_t fibre_count = 20'000u;  // Large enough that at least one queue will resize.
  std::vector<std::pair<int32_t, int32_t>> priorities;
  priorities.reserve(fibre_count);

  std::mt19937 rng(42);
  std::uniform_int_distribution<> dist(-2, 5);
  for (int32_t i = 0; i < fibre_count; ++i)
  {
    const int32_t priority = dist(rng) * 100;
    priorities.emplace_back(i, priority);
  }

  priorityTest(priorities, false);
}

TEST(Fibre, queueResize)
{
  const uint32_t queue_size = 4u;
  SchedulerParams params = { .initial_queue_size = queue_size };
  Scheduler scheduler{ std::move(params) };

  struct SharedState
  {
    uint32_t entered = 0;
    uint32_t completed = 0;
  } state;

  const auto fibre_entry = [](SharedState &state) -> Fibre {
    state.entered++;
    co_yield {};
    state.completed++;
  };

  const uint32_t fibre_count = queue_size * 4;
  for (uint32_t i = 0; i < fibre_count; ++i)
  {
    scheduler.start(fibre_entry(state), std::format("fibre{}", i));
  }

  double elapsed = 0;
  const double dt = 0.1;
  scheduler.update(elapsed += dt);
  EXPECT_EQ(state.entered, fibre_count);
  EXPECT_EQ(state.completed, 0u);
  scheduler.update(elapsed += dt);
  EXPECT_EQ(state.completed, fibre_count);
}
}  // namespace arachne
