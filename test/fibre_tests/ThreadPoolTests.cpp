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
  ThreadPool pool{ ThreadPoolParams{ .thread_count = 4 } };

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
}  // namespace arachne
