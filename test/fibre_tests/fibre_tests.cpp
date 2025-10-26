import fibre;

#include <gtest/gtest.h>

namespace fibre
{
Fibre ticker()
{
  for (int i = 0; i < 5; ++i)
  {
    std::cout << "Tick " << i << '\n';
    co_yield yield();
  }
  std::cout << "Tick done\n";
}

TEST(Fibre, ticker)
{
  System system;
  Id fibre_id = system.start(ticker());

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(system.isRunning(fibre_id));
  while (system.isRunning(fibre_id))
  {
    system.update(simulated_time_s);
    simulated_time_s += dt;
  }

  EXPECT_FALSE(system.isRunning(fibre_id));
}

Fibre cancellation()
{
  struct AtExit
  {
    ~AtExit() { std::cout << "Cancelled!\n"; }
  };

  AtExit at_exit;

  for (int i = 0;; ++i)
  {
    std::cout << "Tick " << i << '\n';
    co_yield yield();
  }
  std::cout << "Tick done\n";
}

TEST(Fibre, cancellation)
{
  System system;
  Id fibre_id = system.start(cancellation());

  const double dt = 0.1;
  double simulated_time_s = 0;
  EXPECT_TRUE(system.isRunning(fibre_id));
  for (int i = 0; i < 5; ++i)
  {
    system.update(simulated_time_s);
    simulated_time_s += dt;
  }

  system.cancel(fibre_id);
  EXPECT_FALSE(system.isRunning(fibre_id));
}

}  // namespace fibre
