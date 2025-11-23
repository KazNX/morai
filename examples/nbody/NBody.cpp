#include <morai/Move.hpp>
#include <morai/Scheduler.hpp>
#include <morai/ThreadPool.hpp>

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

namespace
{
constexpr double QUANTISATION = 1e9;

struct Vec2
{
  double x{};
  double y{};
};

struct Body
{
  Vec2 position{};
  Vec2 velocity{};
  uint32_t idx = 0;
};

struct Render
{
  std::array<std::vector<Vec2>, 2> body_positions;
  uint32_t stamp = 0;
  int32_t read_buffer_idx = 0;
  uint32_t ready_count = 0;
  double dt = 0.0f;
  double target_dt = 0.01f;
  /// Screen display.
  weaver::Screen screen;

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
  {
    render.screen.addLayer(0, { { 0, 0 }, render.screen.size() });
  }
};

struct Options
{
  uint32_t body_count = 100;
};

void calcNBody(Body &body, const std::vector<Vec2> &other_positions, const double dt)
{
  constexpr double G = 6.67430e-11;

  Vec2 acceleration{};

  for (uint32_t i = 0; i < static_cast<uint32_t>(other_positions.size()); ++i)
  {
    if (i == body.idx)
    {
      continue;
    }

    const double dx = other_positions[i].x - body.position.x;
    const double dy = other_positions[i].y - body.position.y;
    const double distance_sq = dx * dx + dy * dy + 1e-10;  // Avoid division by zero.
    const double force = G / distance_sq;                  // Assume unit masses for simplicity.
    acceleration.x += force * dx / std::sqrt(distance_sq);
    acceleration.y += force * dy / std::sqrt(distance_sq);
  }

  // Update velocity and position.
  body.velocity.x += acceleration.x * dt;
  body.velocity.y += acceleration.y * dt;
  body.position.x += body.velocity.x * dt;
  body.position.y += body.velocity.y * dt;
}

morai::Fibre body_fibre(std::shared_ptr<GlobalState> state, Body body)
{
  // Initialisation phase.

  // Main loop
  double dt = 0.0f;
  for (;;)
  {
    // Update phase
    calcNBody(body, state->render.body_positions[state->render.read_buffer_idx], dt);

    // Render phase
    co_await morai::moveTo(state->render_scheduler);

    // Update render position.
    // Wrap position to the screen bounds.
    const auto viewport = state->render.screen.viewport();
    const weaver::Coord clamped_pos = weaver::wrap(
      viewport,
      weaver::Coord{
        static_cast<int>(body.position.x / QUANTISATION) + state->render.screen.size().width / 2,
        static_cast<int>(body.position.y / QUANTISATION) + state->render.screen.size().height / 2,
      });

    // Apply integer clamp to the actual position.
    body.position.x = (static_cast<double>(clamped_pos.x - viewport.size.width / 2)) * QUANTISATION;
    body.position.y =
      (static_cast<double>(clamped_pos.y - viewport.size.height / 2)) * QUANTISATION;

    auto &positions = state->render.body_positions[state->render.writeBufferIdx()];
    positions[body.idx] = body.position;

    ++state->render.ready_count;

    // Wait for frame sync.
    co_await [initial_stamp = state->render.stamp, state]() {
      return state->render.stamp != initial_stamp;
    };

    // Update dt based on the render delay.
    dt = state->render.dt;

    // Move back to the job pool
    co_await morai::moveTo(state->nbody_pool);
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
    for (const auto &pos : positions)
    {
      const weaver::Coord screen_pos = weaver::clamp(
        state->render.screen.viewport(),
        weaver::Coord{
          static_cast<int>(pos.x / QUANTISATION) + state->render.screen.size().width / 2,
          static_cast<int>(pos.y / QUANTISATION) + state->render.screen.size().height / 2,
        });
      state->render.screen.layer(0).setCharacter(screen_pos, weaver::character('.'));
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
  constexpr double SPAWN_RADIUS = 1e11;
  constexpr double MAX_INITIAL_VELOCITY = 1e3;

  state->render.body_positions[0].resize(options.body_count);
  state->render.body_positions[1].resize(options.body_count);

  std::default_random_engine rng(std::random_device{}());
  const auto viewport = state->render.screen.viewport();
  std::uniform_real_distribution<double> pos_dist(0.0, viewport.size.width * QUANTISATION / 2.0);
  std::uniform_real_distribution<double> vel_dist(-MAX_INITIAL_VELOCITY, MAX_INITIAL_VELOCITY);

  for (uint32_t i = 0; i < options.body_count; ++i)
  {
    Body body{
      .position = { pos_dist(rng), pos_dist(rng) },
      .velocity = { vel_dist(rng), vel_dist(rng) },
      .idx = i,
    };

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
       cxxopts::value<uint32_t>(options.body_count)
         ->default_value(std::to_string(options.body_count)))  //
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
                                        .priority_levels = { 0, 1 } };
  std::shared_ptr<GlobalState> state =
    std::make_shared<GlobalState>(std::move(body_pool_params), std::move(render_params));
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
