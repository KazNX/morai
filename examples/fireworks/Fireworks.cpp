#include <arachne/Fibre.hpp>
#include <arachne/Scheduler.hpp>

#include <memory>
#include <weaver/Weaver.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <ranges>
#include <thread>


struct Input
{
  std::vector<weaver::Key> keystrokes;

  [[nodiscard]] bool pressed(weaver::Key key) const
  {
    return std::ranges::find(keystrokes, key) != keystrokes.end();
  }

  void clear() { keystrokes.clear(); }
};

struct GlobalState
{
  arachne::Scheduler scheduler;
  Input input{};
  weaver::Screen screen;
  std::chrono::duration<double> frame_interval{ 1.0 / 60.0 };
  bool quit = false;

  [[nodiscard]] weaver::View &background() { return screen.layer(0); }
  [[nodiscard]] weaver::View &foreground() { return screen.layer(1); }

  GlobalState()
  {
    screen.addLayer(0, { { 0, 0 }, screen.size() });
    screen.addLayer(1, { { 0, 0 }, screen.size() });
  }
};

struct Launcher
{
  weaver::Coord position;
  arachne::Id fibre;
};

arachne::Fibre launcher(std::shared_ptr<GlobalState> state, std::shared_ptr<Launcher> launcher)
{
  launcher->position.x = state->screen.size().width / 2;
  launcher->position.y = state->screen.size().height - 2;

  for (;;)
  {
    if (state->input.pressed(weaver::Key::Comma))
    {
      --launcher->position.x;
    }
    if (state->input.pressed(weaver::Key::Dot))
    {
      ++launcher->position.x;
    }
    if (state->input.pressed(weaver::Key::Space))
    {
    }

    launcher->position.x = std::clamp(launcher->position.x, 0, state->screen.size().width - 1);

    state->foreground().setCharacter(launcher->position, '^');

    co_yield {};
  }
}

arachne::Fibre render(std::shared_ptr<GlobalState> state)
{
  for (;;)
  {
    state->screen.draw();
    state->background().clear();
    state->foreground().clear();
    state->input.keystrokes.clear();
    co_await state->frame_interval;
  }
}

arachne::Fibre input(std::shared_ptr<GlobalState> state)
{
  for (;;)
  {
    for (std::optional<uint32_t> ch = state->screen.input(); ch.has_value();
         ch = state->screen.input())
    {
      state->input.keystrokes.push_back(static_cast<weaver::Key>(*ch));
      if (*ch == 'q' || *ch == 'Q')
      {
        state->quit = true;
      }
    }
    co_yield {};
  }
}

void startFibres(std::shared_ptr<GlobalState> &state)
{
  state->scheduler.start(render(state));
  state->scheduler.start(input(state));
  state->scheduler.start(launcher(state, std::make_shared<Launcher>()));
}

int main()
{
  std::shared_ptr<GlobalState> state = std::make_shared<GlobalState>();

  const auto start_time = std::chrono::steady_clock::now();

  startFibres(state);

  while (!state->quit)
  {
    const auto now = std::chrono::steady_clock::now();
    state->scheduler.update(std::chrono::duration<double>(now - start_time).count());
    std::this_thread::sleep_until(now + state->frame_interval);
  }
}
