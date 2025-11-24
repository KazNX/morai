#include <cstddef>
#include <morai/Move.hpp>
#include <morai/Scheduler.hpp>
#include <morai/ThreadPool.hpp>

#include <morai/Finally.hpp>

#include <weaver/Input.hpp>
#include <weaver/Weaver.hpp>

#include <cxxopts.hpp>

#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>
#include "morai/Common.hpp"
#include "morai/Resumption.hpp"

namespace
{
struct Vec2
{
  double x{};
  double y{};
};

constexpr double QUANTISATION = 1e3;
constexpr double INITIAL_VELOCITY = 1e3;

struct Body
{
  Vec2 position{};
  Vec2 velocity{};
  double mass = 10.0;
  uint32_t idx = 0;
  uint8_t colour = 0;
};

struct Render
{
  std::array<std::vector<Vec2>, 2> body_positions;
  std::vector<double> body_masses;
  std::vector<uint8_t> body_colours;
  std::array<uint8_t, 7> colour_pairs{};
  std::array<double, 7> base_masses{};
  uint32_t stamp = 0;
  int32_t read_buffer_idx = 0;
  uint32_t ready_count = 0;
  double dt = 0.0f;
  double target_dt = 0.01f;
  /// Screen display.
  weaver::Screen screen;

  Render()
  {
    screen.addLayer(0, { { 0, 0 }, screen.size() });
    colour_pairs[0] = screen.defineColour(weaver::Colour::Red, weaver::Colour::Black);
    colour_pairs[1] = screen.defineColour(weaver::Colour::Magenta, weaver::Colour::Black);
    colour_pairs[2] = screen.defineColour(weaver::Colour::Yellow, weaver::Colour::Black);
    colour_pairs[3] = screen.defineColour(weaver::Colour::Green, weaver::Colour::Black);
    colour_pairs[4] = screen.defineColour(weaver::Colour::Blue, weaver::Colour::Black);
    colour_pairs[5] = screen.defineColour(weaver::Colour::Cyan, weaver::Colour::Black);
    colour_pairs[6] = screen.defineColour(weaver::Colour::White, weaver::Colour::Black);

    // Assign masses by colour.
    base_masses[0] = 1e8;
    base_masses[1] = 1e9;
    base_masses[2] = 1e10;
    base_masses[3] = 1e12;
    base_masses[4] = 1e14;
    base_masses[5] = 1e17;
    base_masses[6] = 1e20;
  }

  int writeBufferIdx() const { return 1 - read_buffer_idx; }
  void swapBuffers() { read_buffer_idx = writeBufferIdx(); }
};

struct GlobalState
{
  morai::ThreadPool nbody_pool;
  morai::Scheduler render_scheduler;
  Render render;
  /// Input key handler.
  weaver::Input input{ render.screen };

  GlobalState(morai::ThreadPoolParams body_pool_params, morai::SchedulerParams render_params)
    : nbody_pool{ std::move(body_pool_params) }
    , render_scheduler{ std::move(render_params) }
  {}
};

struct Options
{
  uint32_t body_count = 30;
  double velocity_scale = 1.0;
  double mass_scale = 1.0;
};

void calcNBody(Body &body, const std::vector<Vec2> &body_positions,
               const std::vector<double> &body_masses, const double dt)
{
  constexpr double G = 6.67430e-11;

  Vec2 acceleration{};

  for (uint32_t i = 0; i < static_cast<uint32_t>(body_positions.size()); ++i)
  {
    if (i == body.idx)
    {
      continue;
    }

    const double dx = body_positions.at(i).x - body.position.x;
    const double dy = body_positions.at(i).y - body.position.y;
    const double distance_sq = dx * dx + dy * dy;
    const double force = (distance_sq > 1e-4) ? G * body_masses.at(i) / distance_sq : 0.0;
    acceleration.x += force * dx / std::sqrt(distance_sq);
    acceleration.y += force * dy / std::sqrt(distance_sq);
  }

  // Update velocity and position.
  body.velocity.x += acceleration.x * dt;
  body.velocity.y += acceleration.y * dt;
  body.position.x += body.velocity.x * dt;
  body.position.y += body.velocity.y * dt;
}

void wrap(Body &body, const weaver::Viewport &viewport)
{
  // Build a wrap range in simulation space.
  const Vec2 min_pos{
    static_cast<double>(viewport.origin.x) * QUANTISATION,
    static_cast<double>(viewport.origin.y) * QUANTISATION,
  };
  const Vec2 max_pos{
    static_cast<double>(viewport.origin.x + viewport.size.width) * QUANTISATION,
    static_cast<double>(viewport.origin.y + viewport.size.height) * QUANTISATION,
  };
  const Vec2 range{
    max_pos.x - min_pos.x,
    max_pos.y - min_pos.y,
  };

  if (body.position.x < min_pos.x)
  {
    body.position.x += range.x;
  }
  else if (body.position.x >= max_pos.x)
  {
    body.position.x -= range.x;
  }
  if (body.position.y < min_pos.y)
  {
    body.position.y += range.y;
  }
  else if (body.position.y >= max_pos.y)
  {
    body.position.y -= range.y;
  }
}

morai::Fibre body_fibre(std::shared_ptr<GlobalState> state, Body body)
{
  // Main loop
  double dt = 0.0f;
  for (;;)
  {
    // Update phase
    calcNBody(body, state->render.body_positions[state->render.read_buffer_idx],
              state->render.body_masses, dt);

    // Render phase
    co_await morai::moveTo(state->render_scheduler, 0);

    // Update render position.
    // Wrap position to the screen bounds.
    const auto viewport = state->render.screen.viewport();
    wrap(body, viewport);

    auto &positions = state->render.body_positions[state->render.writeBufferIdx()];
    positions[body.idx] = body.position;

    ++state->render.ready_count;

    // Wait for frame sync.
    co_await [initial_stamp = state->render.stamp, state]() {
      return state->render.stamp != initial_stamp;
    };

    // Reschedule to after render update. That will give an the immediate dt from the current
    // update.
    co_await morai::reschedule(2);

    // RenderUpdate dt based on the render delay.
    dt = state->render.dt;

    // Move back to the job pool
    co_await morai::moveTo(state->nbody_pool, 0);
  }
}

morai::Fibre render_fibre(std::shared_ptr<GlobalState> state)
{
  double last_epoch_time = state->render_scheduler.time().epoch_time_s;
  for (;;)
  {
    co_await [state]() {
      return state->render.ready_count ==
             static_cast<uint32_t>(state->render.body_positions[0].size());
    };
    state->render.ready_count = 0;

    state->render.stamp++;
    state->render.swapBuffers();
    co_yield {};

    // Clear screen
    state->render.screen.clear();

    // Draw bodies
    const auto &positions = state->render.body_positions[state->render.read_buffer_idx];
    for (size_t i = 0; i < positions.size(); ++i)
    {
      const auto &pos = positions.at(i);
      const weaver::Coord screen_pos =
        weaver::clamp(state->render.screen.viewport(), weaver::Coord{
                                                         static_cast<int>(pos.x / QUANTISATION),
                                                         static_cast<int>(pos.y / QUANTISATION),
                                                       });
      state->render.screen.layer(0).setCharacter(
        screen_pos, weaver::character('.', state->render.body_colours.at(i)));
    }

    // Draw to terminal
    state->render.screen.draw();

    state->render.dt = state->render_scheduler.time().epoch_time_s - last_epoch_time;
    last_epoch_time = state->render_scheduler.time().epoch_time_s;

    co_await state->render.target_dt;
  }
}

void createBodies(const Options options, std::shared_ptr<GlobalState> state)
{
  state->render.body_positions[0].resize(options.body_count);
  state->render.body_positions[1].resize(options.body_count);
  state->render.body_masses.resize(options.body_count);
  state->render.body_colours.resize(options.body_count, 0);

  std::default_random_engine rng(std::random_device{}());
  const auto viewport = state->render.screen.viewport();
  std::uniform_real_distribution<double> x_dist(0.0, viewport.size.width);
  std::uniform_real_distribution<double> y_dist(0.0, viewport.size.height);
  std::uniform_real_distribution<double> vel_dist(-INITIAL_VELOCITY * options.velocity_scale,
                                                  INITIAL_VELOCITY * options.velocity_scale);
  std::uniform_int_distribution<int> mass_dist(
    0, static_cast<int>(state->render.colour_pairs.size() - 1));

  for (uint32_t i = 0; i < options.body_count; ++i)
  {
    const int mass_index = mass_dist(rng);
    Body body{
      .position = { x_dist(rng) * QUANTISATION, y_dist(rng) * QUANTISATION },
      .velocity = { vel_dist(rng), vel_dist(rng) },
      .mass = state->render.base_masses[mass_index],
      .idx = i,
      .colour = state->render.colour_pairs.at(mass_index),
    };

    state->render.body_colours.at(i) = body.colour;
    state->render.body_masses.at(i) = body.mass;

    state->nbody_pool.start(body_fibre(state, body));
  }

  // Setup render scheduler to update at ~60Hz.
  state->render_scheduler.start(render_fibre(state), 1, "Render");
}

int parseArgs(Options &options, int argc, char *argv[])
{
  try
  {
    cxxopts::Options cmd_options(
      "NBody",
      R"(A terminal based NBody simulation demonstrating some advanced Morai fibre features.
These are not necessarily good patterns, just demonstrative patterns.

This example demonstrates the use of a thread pool scheduler and moving fibres between schedulers.
)");
    cmd_options.add_options()   //
      ("h,help", "Print help")  //
      ("n,bodies", "Number of bodies to simulate",
       cxxopts::value(options.body_count)->default_value(std::to_string(options.body_count)))  //
      ("v,velocity", "Velocity scale",
       cxxopts::value(options.velocity_scale)
         ->default_value(std::to_string(options.velocity_scale)))  //
      ("m,mass", "Mass scale",
       cxxopts::value(options.mass_scale)->default_value(std::to_string(options.mass_scale)))  //
      ;

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
}  // namespace

int main(int argc, char *argv[])
{
  Options options{};
  if (const int error_code = parseArgs(options, argc, argv); error_code != 0)
  {
    return error_code;
  }

  morai::ThreadPoolParams body_pool_params{ .worker_count = -1 };
  body_pool_params.initial_queue_size = options.body_count * 2;
  morai::SchedulerParams render_params{ .initial_queue_size = options.body_count * 2,
                                        .priority_levels = { 0, 1, 2 } };
  std::shared_ptr<GlobalState> state =
    std::make_shared<GlobalState>(std::move(body_pool_params), std::move(render_params));

  for (auto &base_mass : state->render.base_masses)
  {
    base_mass *= options.mass_scale;
  }

  createBodies(options, state);

  const auto start_time = std::chrono::steady_clock::now();
  bool quit = false;
  while (!quit)
  {
    const auto now = std::chrono::steady_clock::now();
    state->render_scheduler.update(std::chrono::duration<double>(now - start_time).count());
    std::this_thread::sleep_until(now + std::chrono::duration<double>(state->render.target_dt));
    quit = state->input.keyState(weaver::Key::Q) == weaver::KeyState::Down ||
           state->input.keyState(weaver::Key::Escape) == weaver::KeyState::Down;
  }

  state->nbody_pool.cancelAll();
  state->render_scheduler.cancelAll();
  state->nbody_pool.wait();

  return 0;
}
