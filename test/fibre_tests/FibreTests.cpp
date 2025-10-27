import fibre;

#include <gtest/gtest.h>

namespace fibre
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
  struct AtExit
  {
    bool *cleaned_up = nullptr;
    AtExit(bool *cleaned_up)
      : cleaned_up(cleaned_up)
    {}
    ~AtExit()
    {
      if (cleaned_up)
      {
        *cleaned_up = true;
      }
    }
  };

  AtExit at_exit(cleaned_up);

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

}  // namespace fibre
