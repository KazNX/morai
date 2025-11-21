#include <arachne/Finally.hpp>
#include <arachne/ThreadPool.hpp>
#include <chrono>
#include <format>

#include <gtest/gtest.h>

#include <format>

namespace arachne
{
TEST(ThreadPool, simple)
{
  ThreadPool pool{ ThreadPoolParams{ .worker_count = 4 } };

  std::atomic<int> counter = 0;
  const unsigned task_count = 1000;

  const auto task = [&counter]() -> Fibre {
    co_yield {};
    counter.fetch_add(1, std::memory_order_relaxed);
    co_return;
  };

  for (unsigned i = 0; i < task_count; ++i)
  {
    pool.start(task(), std::format("task{}", i));
  }

  EXPECT_TRUE(pool.wait(std::chrono::seconds(5)));
  EXPECT_EQ(counter.load(std::memory_order_relaxed), task_count);
  EXPECT_TRUE(pool.empty());
}

TEST(ThreadPool, zeroWorkers)
{
  ThreadPool pool{ ThreadPoolParams{ .worker_count = 0 } };

  std::atomic<int> counter = 0;
  const unsigned task_count = 100;

  const auto task = [&counter]() -> Fibre {
    co_yield {};
    counter.fetch_add(1, std::memory_order_relaxed);
    co_return;
  };

  for (unsigned i = 0; i < task_count; ++i)
  {
    pool.start(task(), std::format("task{}", i));
  }

  EXPECT_FALSE(pool.wait(std::chrono::milliseconds(100)));
  EXPECT_EQ(counter.load(std::memory_order_relaxed), 0);

  // Now manually update the pool to process tasks.
  pool.update(std::chrono::seconds(5));

  EXPECT_EQ(counter.load(std::memory_order_relaxed), task_count);
  EXPECT_TRUE(pool.empty());
}

TEST(ThreadPool, cancelAll)
{
  ThreadPool pool{ ThreadPoolParams{ .worker_count = 4 } };

  std::atomic<int> counter = 0;
  const unsigned task_count = 1000;

  const auto task = [&counter]() -> Fibre {
    for (;;)
    {
      co_yield {};
      counter.fetch_add(1, std::memory_order_relaxed);
    }
  };

  for (unsigned i = 0; i < task_count; ++i)
  {
    pool.start(task(), std::format("task{}", i));
  }

  // Let the tasks run for a short while.
  EXPECT_FALSE(pool.wait(std::chrono::milliseconds(100)));
  EXPECT_GT(counter.load(std::memory_order_relaxed), 0);

  pool.cancelAll();

  // Ensure all tasks have been cancelled.
  EXPECT_TRUE(pool.wait(std::chrono::milliseconds(100)));
  EXPECT_TRUE(pool.empty());
}
}  // namespace arachne
