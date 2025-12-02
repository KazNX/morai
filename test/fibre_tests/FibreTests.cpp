#include "TestClock.hpp"

#include <morai/Finally.hpp>
#include <morai/Log.hpp>
#include <morai/Scheduler.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <ranges>
#include <random>
#include "morai/Common.hpp"

namespace morai
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
  Scheduler scheduler{ test::makeClock() };
  Id unknown_id{ 9999 };

  EXPECT_FALSE(unknown_id.running());
  EXPECT_FALSE(scheduler.cancel(unknown_id));
}

TEST(Fibre, ticker)
{
  Scheduler scheduler{ test::makeClock() };
  const Id fibre_id = scheduler.start(ticker(), "ticker");

  EXPECT_TRUE(fibre_id.running());
  while (fibre_id.running())
  {
    scheduler.update();
  }

  EXPECT_FALSE(fibre_id.running());
}

Fibre cancellation(bool *cleaned_up)
{
  [[maybe_unused]] const auto at_exit = morai::finally([&cleaned_up]() {
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
  Scheduler scheduler{ test::makeClock() };
  bool cleaned_up = false;
  Id fibre_id = scheduler.start(cancellation(&cleaned_up), "cancellation");

  EXPECT_TRUE(fibre_id.running());
  for (int i = 0; i < 5; ++i)
  {
    scheduler.update();
  }

  EXPECT_TRUE(fibre_id.running());
  scheduler.cancel(fibre_id);
  EXPECT_FALSE(fibre_id.running());

  // Start a new fibre, but cancel via the Id instead.
  fibre_id = scheduler.start(cancellation(&cleaned_up), "cancellation2");
  EXPECT_TRUE(fibre_id.running());
  for (int i = 0; i < 5; ++i)
  {
    scheduler.update();
  }

  EXPECT_TRUE(fibre_id.running());
  fibre_id.markForCancellation();
  // Stays running until the next update.
  EXPECT_TRUE(fibre_id.running());
  scheduler.update();
  EXPECT_FALSE(fibre_id.running());
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

  Scheduler scheduler{ test::makeClock(0.01) };
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

  for (state.time = scheduler.clock().epoch(); state.time < 1.0 && !scheduler.empty();
       state.time += scheduler.clock().epoch())
  {
    scheduler.update();
  }

  EXPECT_TRUE(state.signal);
  EXPECT_TRUE(scheduler.empty());
  EXPECT_LE(state.signaller_end_time, state.waiter_end_time);
}

TEST(Fibre, spawn)
{
  Scheduler scheduler{ test::makeClock() };

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

    EXPECT_FALSE(child1.running());
    EXPECT_FALSE(child2.running());

    std::cout << "Parent fibre done\n";
  };

  Id parent_id = scheduler.start(parent_fibre(), "parent");

  EXPECT_TRUE(parent_id.running());
  while (parent_id.running())
  {
    scheduler.update();
  }

  EXPECT_FALSE(parent_id.running());
  EXPECT_TRUE(scheduler.empty());
}


TEST(Fibre, spawnAndCancel)
{
  Scheduler scheduler{ test::makeClock() };

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

    EXPECT_TRUE(child1.running());
    EXPECT_FALSE(child2.running());
    scheduler.cancel(child1);

    std::cout << "Parent fibre done\n";
  };

  const Id parent_id = scheduler.start(parent_fibre(), "parent");
  const Id persistent_id = scheduler.start(child_fibre(99, true), "persistent");

  while (parent_id.running())
  {
    scheduler.update();
  }

  EXPECT_FALSE(parent_id.running());
  EXPECT_TRUE(persistent_id.running());
  scheduler.cancelAll();
  EXPECT_TRUE(scheduler.empty());
}

TEST(Fibre, exceptionPropagation)
{
  Scheduler scheduler{ test::makeClock(), ExceptionHandling::Rethrow };

  const auto faulty_fibre = []() -> Fibre {
    std::cout << "Faulty fibre started\n";
    co_yield {};
    throw std::runtime_error("Something went wrong in the fibre");
    co_yield {};
  };

  Id fibre_id = scheduler.start(faulty_fibre(), "faulty");

  EXPECT_TRUE(fibre_id.running());
  bool exception_caught = false;
  while (fibre_id.running())
  {
    try
    {
      scheduler.update();
    }
    catch (const std::runtime_error &e)
    {
      std::cout << "Caught exception from fibre: " << e.what() << '\n';
      exception_caught = true;
    }
  }

  EXPECT_FALSE(fibre_id.running());
  EXPECT_TRUE(exception_caught);

  // Ensure log mode does not throw.
  scheduler.setExceptionHandling(ExceptionHandling::Log);
  fibre_id = scheduler.start(faulty_fibre(), "faulty");

  EXPECT_TRUE(fibre_id.running());
  exception_caught = false;
  while (fibre_id.running())
  {
    try
    {
      scheduler.update();
    }
    catch (const std::runtime_error &e)
    {
      std::cout << "Caught exception from fibre: " << e.what() << '\n';
      exception_caught = true;
    }
  }

  EXPECT_FALSE(fibre_id.running());
  EXPECT_FALSE(exception_caught);
}

void priorityTest(std::span<std::pair<int32_t, int32_t>> id_priority_pairs, bool const log = false,
                  const uint32_t queue_size = 1024u)
{
  // Track log errors to look for priority mismatches.
  uint32_t log_failures = 0;
  const auto log_hook = [&log_failures](log::Level level, std::string_view msg) {
    if (level == log::Level::Error)
    {
      ++log_failures;
    }
    std::cout << msg << '\n';
  };

  log::setHook(log_hook);
  const auto clear_hook = finally([]() { log::clearHook(); });

  SchedulerParams params;
  params.initial_queue_size = queue_size;
  for (const auto &pair : id_priority_pairs)
  {
    if (std::ranges::find(params.priority_levels, pair.second) == params.priority_levels.end())
    {
      params.priority_levels.emplace_back(pair.second);
    }
  }

  Scheduler scheduler{ { test::makeClock() }, std::move(params) };
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

  for (const auto &id_priority_pair : id_priority_pairs)
  {
    scheduler.start(fibre_entry(id_priority_pair.first), id_priority_pair.second,
                    std::format("fibre{}", id_priority_pair.first));
  }

  scheduler.update();
  scheduler.update();

  std::ranges::sort(id_priority_pairs,
                    [](const std::pair<int32_t, int32_t> &a, const std::pair<int32_t, int32_t> &b) {
                      return a.second < b.second || a.second == b.second && a.first < b.first;
                    });

  // Resolve the expected order.
  std::vector<int32_t> expected_order;
  for (const auto idx :
       id_priority_pairs | std::views::transform([](const auto &pair) { return pair.first; }))
  {
    expected_order.emplace_back(idx);
  }

  EXPECT_EQ(execution_order, expected_order);
  EXPECT_EQ(0, log_failures);
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
  Scheduler scheduler{ { test::makeClock() }, std::move(params) };

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

  scheduler.update();
  EXPECT_EQ(state.entered, fibre_count);
  EXPECT_EQ(state.completed, 0u);
  scheduler.update();
  EXPECT_EQ(state.completed, fibre_count);
}

TEST(Fibre, incorrectPriority)
{
  // Track log errors to look for priority mismatches.
  uint32_t log_failures = 0;
  const auto log_hook = [&log_failures](log::Level level, std::string_view msg) {
    if (level == log::Level::Error)
    {
      ++log_failures;
    }
    std::cout << msg << '\n';
  };

  log::setHook(log_hook);
  const auto clear_hook = finally([]() { log::clearHook(); });

  Scheduler scheduler{ { test::makeClock() }, { .priority_levels = { -1, 1, 2 } } };

  scheduler.start(ticker(), 0);   // Mismatch.
  scheduler.start(ticker(), -2);  // Mismatch.
  scheduler.start(ticker(), 5);   // Mismatch.
  scheduler.start(ticker(), 1);   // ok.

  EXPECT_EQ(log_failures, 3);
}

TEST(Fibre, capture)
{
  Scheduler scheduler;
  auto state = std::make_shared<int>(42);

  scheduler.start(
    [](std::shared_ptr<int> state) -> Fibre {
      // State variable is definitely copied into the fibre frame and remains valid for the life
      // of the fibre.
      std::cout << "Better: " << *state << std::endl;
      co_return;
    }(state),
    "Better");

  scheduler.start(
    [&state]() -> Fibre {
      // State likely to be valid once the fibre starts, by may become invalid as the fibre
      // continues.
      std::cout << "Risky: " << *state << std::endl;
      co_return;
    }(),
    "Risky");

#if 0
  scheduler.start(
    [state]() -> Fibre {
      // Buy the time the fibre starts, the state variable may have been disposed.
      // This seems to come about if the fibre is moved before it starts.
      std::cout << "Bad: " << *state << std::endl;
      co_return;
    }(),
    "Bad");
#endif  // 0

  scheduler.update();
}
}  // namespace morai
