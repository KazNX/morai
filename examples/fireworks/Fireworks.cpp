#include <setjmp.h>
#include <arachne/Fibre.hpp>
#include <arachne/Scheduler.hpp>

#include <memory>
#include <random>
#include <weaver/Weaver.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <numbers>
#include <random>
#include <ranges>
#include <thread>
#include <type_traits>


template <typename T>
  requires std::is_arithmetic_v<T>
struct Range
{
  T min{};
  T max{};
};

constexpr float FIZZLE_DURATION = 0.6f;
constexpr float FIZZLE_SHORT_DURATION = 0.2f;
constexpr float FIZZLE_STDDEV = 0.3f;
constexpr Range SPARKS{ 10, 20 };            // Number of sparks to spawn
constexpr Range SPARK_DELAY{ 0.0f, 0.05f };  // Delay between spawning sparks
constexpr Range SPARK_SPEED{ 5.0f, 10.0f };  // Initial spark speed
constexpr Range SPARK_LIFETIME{ 1.5f, 2.5f };

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
  std::mt19937 rng{ std::random_device{}() };
  std::array<uint8_t, 8> colour_pairs{};
  float gravity = 5.f;
  bool quit = false;

  [[nodiscard]] weaver::View &background() { return screen.layer(0); }
  [[nodiscard]] weaver::View &foreground() { return screen.layer(1); }

  GlobalState()
  {
    screen.addLayer(0, { { 0, 0 }, screen.size() });
    screen.addLayer(1, { { 0, 0 }, screen.size() });

    colour_pairs[0] = screen.defineColour(weaver::Colour::Black, weaver::Colour::White);
    colour_pairs[1] = screen.defineColour(weaver::Colour::Red, weaver::Colour::Black);
    colour_pairs[2] = screen.defineColour(weaver::Colour::Green, weaver::Colour::Black);
    colour_pairs[3] = screen.defineColour(weaver::Colour::Yellow, weaver::Colour::Black);
    colour_pairs[4] = screen.defineColour(weaver::Colour::Blue, weaver::Colour::Black);
    colour_pairs[5] = screen.defineColour(weaver::Colour::Magenta, weaver::Colour::Black);
    colour_pairs[6] = screen.defineColour(weaver::Colour::Cyan, weaver::Colour::Black);
    colour_pairs[7] = screen.defineColour(weaver::Colour::White, weaver::Colour::Black);
  }
};

struct Launcher
{
  weaver::Coord position;
  arachne::Id fibre;
};

struct Spark
{
  // Quanitsation unit for fixed point position/velocity
  inline static int SPARK_UNIT = 1'000;
  weaver::Coord position{};
  weaver::Coord velocity{};
  float lifetime = 0;
};

arachne::Fibre fizzle(std::shared_ptr<GlobalState> state, weaver::Coord position, float duration)
{
  const std::array sprite = {
    weaver::character('.', state->colour_pairs[1]), weaver::character('.', state->colour_pairs[2]),
    weaver::character('.', state->colour_pairs[3]), weaver::character('.', state->colour_pairs[4]),
    weaver::character('.', state->colour_pairs[5]), weaver::character('.', state->colour_pairs[6]),
    weaver::character('.', state->colour_pairs[7]),
  };
  std::uniform_int_distribution sprite_dist(0, static_cast<int>(sprite.size() - 1));

  while (duration > 0)
  {
    const size_t index =
      static_cast<size_t>((1.0f - (duration / duration)) * static_cast<float>(sprite.size() - 1));
    state->background().setCharacter(position, sprite[sprite_dist(state->rng)]);
    co_yield {};
    duration -= static_cast<float>(state->scheduler.time().dt);
  }
}

arachne::Fibre spark(std::shared_ptr<GlobalState> state, Spark spark, weaver::Colour colour)
{
  const uint8_t colour_pair =
    state->colour_pairs[static_cast<size_t>(colour) % state->colour_pairs.size()];
  const std::array sprite{ weaver::character('*', colour_pair), weaver::character('+', colour_pair),
                           weaver::character('x', colour_pair),
                           weaver::character('.', colour_pair) };

  std::uniform_int_distribution sprite_dist(0, static_cast<int>(sprite.size() - 1));

  while (spark.lifetime > 0)
  {
    weaver::View &view = state->foreground();
    const weaver::Coord pos =
      weaver::clamp(view.viewport(),
                    { spark.position.x / Spark::SPARK_UNIT, spark.position.y / Spark::SPARK_UNIT });
    view.setCharacter(pos, sprite[sprite_dist(state->rng)]);
    co_yield {};

    // Drop a fizzle at the previous position.
    state->scheduler.start(fizzle(state, pos, FIZZLE_DURATION));

    const auto dt = static_cast<float>(state->scheduler.time().dt);

    spark.position.x += static_cast<int>(static_cast<float>(spark.velocity.x) * dt);
    spark.position.y += static_cast<int>(static_cast<float>(spark.velocity.y) * dt);
    spark.velocity.y +=
      static_cast<int>(state->gravity * dt * static_cast<float>(Spark::SPARK_UNIT));
    spark.lifetime -= dt;
  }

  std::normal_distribution fizzle_dist(FIZZLE_DURATION, FIZZLE_STDDEV);
  const weaver::Coord pos =
    weaver::clamp(state->foreground().viewport(),
                  { spark.position.x / Spark::SPARK_UNIT, spark.position.y / Spark::SPARK_UNIT });
  state->scheduler.start(fizzle(state, pos, fizzle_dist(state->rng)));
}

arachne::Fibre explode(std::shared_ptr<GlobalState> state, weaver::Coord position,
                       weaver::Colour colour)
{
  // Spawn half the sparks immediately, then intermittently spawn the rest
  std::uniform_int_distribution spark_count_dist(SPARKS.min, SPARKS.max);
  const int total_sparks = spark_count_dist(state->rng);

  std::uniform_real_distribution speed_dist(SPARK_SPEED.min, SPARK_SPEED.max);
  std::uniform_real_distribution lifetime_dist(SPARK_LIFETIME.min, SPARK_LIFETIME.max);

  float angle = 0.0f;
  const float angle_step = (2.0f * std::numbers::pi_v<float>) / static_cast<float>(total_sparks);
  for (int i = 0; i < total_sparks; ++i, angle += angle_step)
  {
    const float speed = speed_dist(state->rng);
    state->scheduler.start(
      spark(state,
            Spark{ .position = position * Spark::SPARK_UNIT,
                   .velocity = { static_cast<int>(speed * std::cos(angle) * Spark::SPARK_UNIT),
                                 static_cast<int>(speed * std::sin(angle) * Spark::SPARK_UNIT) },
                   .lifetime = lifetime_dist(state->rng) },
            colour));
  }

  co_return;
}

arachne::Fibre rocket(std::shared_ptr<GlobalState> state, weaver::Coord position)
{
  // Choose a random colour, but not black.
  const auto colour = static_cast<weaver::Colour>(
    std::uniform_int_distribution{ 1, static_cast<int>(weaver::Colour::White) }(state->rng));
  const auto sprite = weaver::character('|', state->colour_pairs[static_cast<int>(colour)]);

  const int height = state->screen.size().height;
  auto dist = std::normal_distribution<float>{ 3.0f * height / 4.0f, height / 10.0f };
  const auto target_height = height - static_cast<int>(dist(state->rng));

  // Ascend
  while (position.y > target_height)
  {
    state->foreground().setCharacter({ position.x, position.y }, sprite);
    --position.y;
    co_yield {};
  }

  state->scheduler.start(explode(state, position, colour));
}

arachne::Fibre launcher(std::shared_ptr<GlobalState> state, std::shared_ptr<Launcher> launcher)
{
  launcher->position.x = state->screen.size().width / 2;
  launcher->position.y = state->screen.size().height - 2;

  for (;;)
  {
    if (state->input.pressed(weaver::Key::A) || state->input.pressed(weaver::Key::a))
    {
      --launcher->position.x;
    }
    if (state->input.pressed(weaver::Key::D) || state->input.pressed(weaver::Key::d))
    {
      ++launcher->position.x;
    }
    if (state->input.pressed(weaver::Key::Space))
    {
      state->scheduler.start(rocket(state, launcher->position));
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
