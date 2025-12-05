#include <morai/Finally.hpp>
#include <morai/ThreadPool.hpp>
#include <chrono>
#include <format>

#include <gtest/gtest.h>

#include <format>

namespace morai
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

// FIXME: This test deadlocks as the job queues cannot grow. This is a known limitation.
TEST(ThreadPool, smallQueue)
{
  ThreadPoolParams params{ .worker_count = 2 };
  params.initial_queue_size = 2;
  ThreadPool pool{ params };

  std::atomic<int> counter = 0;
  std::atomic_flag block = ATOMIC_FLAG_INIT;
  const unsigned task_count = 40;

  block.test_and_set();

  const auto task = [&counter, &block]() -> Fibre {
    co_yield {};
    counter.fetch_add(1, std::memory_order_relaxed);
    while (block.test())
    {
      co_yield {};
    }
    co_return;
  };

  // Start a thread to clear the block flag after a delay. The test is working if it completes.
  std::thread unblocker_thread([&block]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    block.clear();
  });

  // Try start all fibres.
  for (unsigned i = 0; i < task_count; ++i)
  {
    pool.start(task(), std::format("task{}", i));
  }

  block.clear();

  EXPECT_TRUE(pool.wait(std::chrono::seconds(5)));
  EXPECT_EQ(counter.load(std::memory_order_relaxed), task_count);
  EXPECT_TRUE(pool.empty());

  unblocker_thread.join();
}
}  // namespace morai
