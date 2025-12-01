#include <morai/Scheduler.hpp>

#include <weaver/Input.hpp>
#include <weaver/Weaver.hpp>

#include <cxxopts.hpp>

#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <numbers>
#include <random>
#include <thread>
#include <type_traits>

// Utility type
template <typename T>
  requires std::is_arithmetic_v<T>
struct Range
{
  T min{};
  T max{};
};

// Constants
constexpr double LAUNCH_DELAY = 0.1;
constexpr Range AUTO_LAUNCH_WINDOW = { 0.2, 2.0 };
constexpr float FIZZLE_DURATION = 0.6f;
constexpr float FIZZLE_STDDEV = 0.3f;
constexpr float GRAVITY = 3.0f;
constexpr Range SPARKS{ 10, 20 };            // Number of sparks to spawn
constexpr Range SPARK_SPEED{ 5.0f, 10.0f };  // Initial spark speed
constexpr Range SPARK_LIFETIME{ 1.5f, 2.5f };

constexpr auto MODE_SWITCH_KEY = weaver::Key::Tab;

// Global state shared between fibres.
struct GlobalState
{
  /// Fibre scheduler. Only one is used.
  morai::Scheduler scheduler;
  /// Screen display.
  weaver::Screen screen;
  /// Input key handler.
  weaver::Input input{ screen };
  /// Target frame interval.
  std::chrono::duration<double> frame_interval{ 1.0 / 60.0 };
  /// Shared random number generator.
  std::mt19937 rng{ std::random_device{}() };
  /// Colour pair definitions.
  std::array<uint8_t, 8> colour_pairs{};
  /// Quit flag.
  bool quit = false;

  /// Get the background rendering layer.
  [[nodiscard]] weaver::View &background() { return screen.layer(0); }
  /// Get the foreground rendering layer.
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

/// Firework launcher state variables.
struct Launcher
{
  weaver::Coord position;
  morai::Id fibre;
};

/// Spark(le) state variables.
struct Spark
{
  /// Quanitsation unit for fixed point position/velocity
  inline static int SPARK_UNIT = 1'000;
  weaver::Coord position{};
  weaver::Coord velocity{};
  float lifetime = 0;
};

/// Starts a fizzle effect at the given position. Sparkles for @p duration before expiring.
morai::Fibre fizzle(std::shared_ptr<GlobalState> state, weaver::Coord position, float duration)
{
  const std::array sprites = {
    weaver::character('.', state->colour_pairs[1]),  //
    weaver::character('.', state->colour_pairs[2]),  //
    weaver::character('.', state->colour_pairs[3]),  //
    weaver::character('.', state->colour_pairs[4]),  //
    weaver::character('.', state->colour_pairs[5]),  //
    weaver::character('.', state->colour_pairs[6]),  //
    weaver::character('.', state->colour_pairs[7]),  //
    // Give some chance to be blank.
    weaver::character('\0'),  //
    weaver::character('\0'),  //
    weaver::character('\0'),  //
    weaver::character('\0'),  //
  };
  std::uniform_int_distribution sprite_dist(0, static_cast<int>(sprites.size() - 1));

  while (duration > 0)
  {
    const auto sprite = sprites[sprite_dist(state->rng)];
    state->background().setCharacter(position, sprite);
    co_yield {};
    duration -= static_cast<float>(state->scheduler.time().dt);
  }
}

/// Starts a firework spark, which moves at the given velocity with gravity applied.
/// Drops @c fizzle() effects as it moves and expires.
morai::Fibre spark(std::shared_ptr<GlobalState> state, Spark spark, weaver::Colour colour)
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
    const weaver::Coord pos = { spark.position.x / Spark::SPARK_UNIT,
                                spark.position.y / Spark::SPARK_UNIT };
    if (weaver::contains(view.viewport(), pos))
    {
      view.setCharacter(pos, sprite[sprite_dist(state->rng)]);
      co_yield {};
      // Drop a fizzle at the previous position.
      state->scheduler.start(fizzle(state, pos, FIZZLE_DURATION));
    }
    else
    {
      co_yield {};
    }

    const auto dt = static_cast<float>(state->scheduler.time().dt);

    spark.position.x += static_cast<int>(static_cast<float>(spark.velocity.x) * dt);
    spark.position.y += static_cast<int>(static_cast<float>(spark.velocity.y) * dt);
    spark.velocity.y += static_cast<int>(GRAVITY * dt * static_cast<float>(Spark::SPARK_UNIT));
    spark.lifetime -= dt;
  }

  const weaver::Coord pos = { spark.position.x / Spark::SPARK_UNIT,
                              spark.position.y / Spark::SPARK_UNIT };
  if (weaver::contains(state->foreground().viewport(), pos))
  {
    std::normal_distribution fizzle_dist(FIZZLE_DURATION, FIZZLE_STDDEV);
    state->scheduler.start(fizzle(state, pos, fizzle_dist(state->rng)));
  }
}

/// Firework explosion. Spawns @c spark() fibres.
morai::Fibre explode(std::shared_ptr<GlobalState> state, weaver::Coord position,
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

/// Firework rocket fibre. Ascends then explodes.
morai::Fibre rocket(std::shared_ptr<GlobalState> state, weaver::Coord position)
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

/// User controller launcher fibre.
morai::Fibre launcherUser(std::shared_ptr<GlobalState> state, std::shared_ptr<Launcher> launcher)
{
  double last_launch_time = -1.0;

  for (;;)
  {
    // Switch to auto mode?
    if (state->input.keyState(MODE_SWITCH_KEY) == weaver::KeyState::Down)
    {
      // Wait for the key release to prevent bouncing.
      co_await [state]() { return state->input.keyState(MODE_SWITCH_KEY) == weaver::KeyState::Up; };
      co_return;
    }

    if (state->input.anyKeyDown({ weaver::Key::A, weaver::Key::ArrowLeft }))
    {
      --launcher->position.x;
    }
    if (state->input.anyKeyDown({ weaver::Key::D, weaver::Key::ArrowRight }))
    {
      ++launcher->position.x;
    }
    if (state->input.anyKeyDown({ weaver::Key::Space, weaver::Key::Enter }))
    {
      const double current_time = state->scheduler.time().epoch_time_s;
      if (current_time - last_launch_time >= LAUNCH_DELAY)
      {
        last_launch_time = current_time;
        state->scheduler.start(rocket(state, launcher->position));
      }
    }

    launcher->position = weaver::clamp(state->screen.viewport(), launcher->position);

    state->foreground().setCharacter(launcher->position, '^');

    co_yield {};
  }
}

/// Autonatic launcher fibre.
morai::Fibre launcherAuto(std::shared_ptr<GlobalState> state, std::shared_ptr<Launcher> launcher)
{
  std::uniform_int_distribution<int> position_dist(1, state->screen.size().width - 2);
  std::uniform_real_distribution<double> launch_delay_dist(AUTO_LAUNCH_WINDOW.min,
                                                           AUTO_LAUNCH_WINDOW.max);

  double next_launch_time = state->scheduler.time().epoch_time_s + launch_delay_dist(state->rng);

  for (;;)
  {
    // Switch to manual mode?
    if (state->input.keyState(MODE_SWITCH_KEY) == weaver::KeyState::Down)
    {
      // Wait for the key release to prevent bouncing.
      co_await [state]() { return state->input.keyState(MODE_SWITCH_KEY) == weaver::KeyState::Up; };
      co_return;
    }

    const double current_time = state->scheduler.time().epoch_time_s;
    if (current_time >= next_launch_time)
    {
      launcher->position.x = position_dist(state->rng);
      state->scheduler.start(rocket(state, launcher->position));
      next_launch_time = state->scheduler.time().epoch_time_s + launch_delay_dist(state->rng);
    }

    state->foreground().setCharacter(launcher->position, '^');

    co_yield {};
  }
}

/// Launcher selector. Toggles running user and automatic launchers as one expires.
morai::Fibre launcher(std::shared_ptr<GlobalState> state, std::shared_ptr<Launcher> launcher)
{
  launcher->position.x = state->screen.size().width / 2;
  launcher->position.y = state->screen.size().height - 2;

  for (;;)
  {
    // Start with the user mode launcher. If that ends we'll switch to auto mode and we basically
    // toggle between them.
    co_await state->scheduler.start(launcherUser(state, launcher));
    co_await state->scheduler.start(launcherAuto(state, launcher));
  }
}

/// Rendering fibre.
morai::Fibre render(std::shared_ptr<GlobalState> state)
{
  for (;;)
  {
    state->screen.draw();
    state->background().clear();
    state->foreground().clear();
    co_await state->frame_interval;
  }
}

/// Input handling fibre.
morai::Fibre input(std::shared_ptr<GlobalState> state)
{
  for (;;)
  {
    state->input.poll();
    if (state->input.anyKeyDown({ weaver::Key::Q, weaver::Key::Escape }))
    {
      state->quit = true;
    }
    co_yield {};
  }
}

/// Start the core fibres.
void startFibres(std::shared_ptr<GlobalState> &state)
{
  state->scheduler.start(render(state));
  state->scheduler.start(input(state));
  state->scheduler.start(launcher(state, std::make_shared<Launcher>()));
}

struct Options
{
  // Nothing right now.
};

int parseArgs([[maybe_unused]] Options &options, int argc, char *argv[])
{
  try
  {
    cxxopts::Options cmd_options(
      "Fireworks",
      R"(A terminal based fireworks display showing some basic Morai fibre usage patterns.
These are not necessarily good patterns, just demonstrative patterns.

Keys:
  A/D or Left/Right Arrow: Move launcher (manual mode)
  Space or Enter: Launch firework (manual mode)
  Tab: Toggle between manual and auto launch modes
  Q or Escape: Quit

Note: Windows provides continuous, multi-key input. Other platforms rely on key repeat.
)");
    cmd_options.add_options()("h,help", "Print help");

    const auto result = cmd_options.parse(argc, argv);

    if (result.count("help"))
    {
      std::cout << cmd_options.help() << std::endl;
      return 1;
    }
  }
  catch (const cxxopts::exceptions::exception &e)
  {
    std::cerr << "Error parsing options: " << e.what() << std::endl;
    return 2;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  Options options{};
  if (const int error_code = parseArgs(options, argc, argv); error_code != 0)
  {
    return error_code;
  }

  std::shared_ptr<GlobalState> state = std::make_shared<GlobalState>();

  startFibres(state);

  while (!state->quit)
  {
    const auto now = std::chrono::steady_clock::now();
    state->scheduler.update();
    std::this_thread::sleep_until(now + state->frame_interval);
  }
}
