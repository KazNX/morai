#include <cstdint>
#include <morai/Move.hpp>
#include <morai/Scheduler.hpp>

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

namespace
{
struct GlobalState
{
  static constexpr uint8_t EditModeBits = 3;
  static constexpr uint8_t SimBit0 = 1;
  static constexpr uint8_t SimBit1 = 2;

  std::vector<uint8_t> cells;
  morai::Scheduler scheduler;
  std::array<uint8_t, 8> colour_pairs{};
  weaver::Screen screen;
  /// Input key handler.
  weaver::Input input{ screen };
  weaver::Coord cursor_pos{ screen.size().width / 2, screen.size().height / 2 };
  /// The bit to check for the current generation (read access).
  uint8_t current_generation_bit = EditModeBits;
  /// The bit to set for the next generation (write access).
  /// Starts in non-simulation mode ({1,2} when simulating, opposite of current_generation_bit).
  uint8_t next_generation_bit = EditModeBits;
  bool quit = false;

  bool editMode() const { return next_generation_bit == current_generation_bit; }

  void setEditMode(bool edit)
  {
    if (edit != editMode())
    {
      if (edit)
      {
        // Switching to edit mode. Copy the current generation into the next then set the generation
        // bits to edit mode.
        for (auto &cell : cells)
        {
          const bool alive = (cell & current_generation_bit) != 0;
          cell &= ~EditModeBits;           // Clear the target bits
          cell |= !!alive * EditModeBits;  // Set the target bits to current generation
        }
        next_generation_bit = current_generation_bit = EditModeBits;
      }
      else
      {
        // In sim mode, the current and next get bits differ and swap each generation.
        current_generation_bit = SimBit0;
        next_generation_bit = SimBit1;
      }
    }
  }

  void randomise()
  {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> alive_dist(0, 1);

    for (auto &cell : cells)
    {
      cell &= ~EditModeBits;  // Clear the target bit
      cell |= EditModeBits * !!(alive_dist(rng));
    }
  }

  void toggleCell(const weaver::Coord position)
  {
    const size_t index = position.x + position.y * screen.size().width;
    uint8_t &cell = cells.at(index);
    // Set next bit if current bit is not set.
    const uint8_t new_bit = next_generation_bit * !(cell & current_generation_bit);
    cell &= ~next_generation_bit;  // Clear the target bit
    cell |= new_bit;               // Set the new bit.
  }

  void setCell(const weaver::Coord position, const bool alive)
  {
    const size_t index = position.x + position.y * screen.size().width;
    uint8_t &cell = cells.at(index);
    cell &= ~next_generation_bit;  // Clear the target bit
    cell |= next_generation_bit * !!alive;
  }

  bool cell(const weaver::Coord position)
  {
    const size_t index = position.x + position.y * screen.size().width;
    return (cells.at(index) & current_generation_bit) != 0;
  }

  void nextGeneration() { std::swap(current_generation_bit, next_generation_bit); }

  GlobalState(morai::SchedulerParams render_params)
    : scheduler{ std::move(render_params) }
  {
    screen.addLayer(0, { { 0, 0 }, screen.size() });
    cells.resize(screen.size().width * screen.size().height, false);
    colour_pairs[0] = screen.defineColour(weaver::Colour::White, weaver::Colour::Black);
    colour_pairs[1] = screen.defineColour(weaver::Colour::Black, weaver::Colour::White);
    colour_pairs[2] = screen.defineColour(weaver::Colour::Magenta, weaver::Colour::Black);
    colour_pairs[3] = screen.defineColour(weaver::Colour::Red, weaver::Colour::Black);
    colour_pairs[4] = screen.defineColour(weaver::Colour::Yellow, weaver::Colour::Black);
    colour_pairs[5] = screen.defineColour(weaver::Colour::Green, weaver::Colour::Black);
    colour_pairs[6] = screen.defineColour(weaver::Colour::Blue, weaver::Colour::Black);
    colour_pairs[7] = screen.defineColour(weaver::Colour::Cyan, weaver::Colour::Black);
  }
};

struct Options
{
  bool edit = false;
};

int parseArgs(Options &options, int argc, char *argv[])
{
  try
  {
    cxxopts::Options cmd_options("Conway",
                                 R"(A terminal based Conway's game of life using Morai fibres.
These are not necessarily good patterns, just demonstrative patterns.
)");
    cmd_options.add_options()   //
      ("h,help", "Print help")  //
      ("e,edit", "Start in edit mode.",
       cxxopts::value(options.edit)->implicit_value("true"))  //
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

morai::Fibre conwayCellFibre(std::shared_ptr<GlobalState> state, weaver::Coord position)
{
  const weaver::Viewport view = state->screen.viewport();

  // Simple random initial state.
  for (;;)
  {
    co_await [state]() { return !state->editMode(); };

    // Count alive neighbours.
    int alive_neighbours = 0;
    for (int dy = -1; dy <= 1; ++dy)
    {
      for (int dx = -1; dx <= 1; ++dx)
      {
        if (dx == 0 && dy == 0)
        {
          continue;
        }

        const auto neighbour_pos =
          weaver::wrap(view, weaver::Coord{ position.x + dx, position.y + dy });
        const bool neighbour_alive = state->cell(neighbour_pos);

        alive_neighbours += neighbour_alive ? 1 : 0;
      }
    }

    bool alive = state->cell(position);

    // Apply Conway's rules.
    // 1. Any live cell with fewer than two live neighbours dies, as if by underpopulation.
    // 2. Any live cell with two or three live neighbours lives on to the next generation.
    // 3. Any live cell with more than three live neighbours dies, as if by overpopulation.
    // 4. Any dead cell with exactly three live neighbours becomes a live cell, as if by
    // reproduction.
    if (alive)
    {
      // 1, 2, 3
      if (alive_neighbours < 2 || alive_neighbours > 3)
      {
        alive = false;
      }
    }
    else if (alive_neighbours == 3)
    {
      alive = true;
    }

    state->setCell(position, alive);

    co_yield {};
  }
}

morai::Fibre renderFibre(std::shared_ptr<GlobalState> state)
{
  for (;;)
  {
    state->input.poll();
    weaver::View &view = state->screen.layer(0);

    for (int y = 0; y < state->screen.size().height; ++y)
    {
      for (int x = 0; x < state->screen.size().width; ++x)
      {
        const weaver::Coord position{ x, y };
        const bool alive = state->cell(position);
        // Use different character for the cursor when not simulating.
        char ch = (!state->editMode() || position != state->cursor_pos) ? ' ' : '+';
        uint8_t colour_pair = (alive) ? state->colour_pairs[1] : state->colour_pairs[0];
        view.setCharacter(position, weaver::character(ch, colour_pair));
      }
    }

    state->nextGeneration();

    state->screen.draw();
    co_yield {};
  }
}

morai::Fibre inputFibre(std::shared_ptr<GlobalState> state)
{
  weaver::Coord &cursor_pos = state->cursor_pos;

  for (;;)
  {
    state->input.poll();
    if (state->input.anyKeyDown({ weaver::Key::Q, weaver::Key::Escape }))
    {
      state->quit = true;
    }

    if (state->editMode())
    {
      if (state->input.anyKeyDown({ weaver::Key::W, weaver::Key::ArrowUp }))
      {
        --cursor_pos.y;
      }
      if (state->input.anyKeyDown({ weaver::Key::S, weaver::Key::ArrowDown }))
      {
        ++cursor_pos.y;
      }
      if (state->input.anyKeyDown({ weaver::Key::A, weaver::Key::ArrowLeft }))
      {
        --cursor_pos.x;
      }
      if (state->input.anyKeyDown({ weaver::Key::D, weaver::Key::ArrowRight }))
      {
        ++cursor_pos.x;
      }

      if (state->input.keyState(weaver::Key::Space) == weaver::KeyState::Down ||
          state->input.keyState(weaver::Key::X) == weaver::KeyState::Down)
      {
        state->toggleCell(cursor_pos);
      }

      if (state->input.anyKeyDown({ weaver::Key::C }))
      {
        // Clear the board.
        std::fill(state->cells.begin(), state->cells.end(), false);
      }

      if (state->input.anyKeyDown({ weaver::Key::R }))
      {
        state->randomise();
      }

      cursor_pos = weaver::wrap(state->screen.viewport(), cursor_pos);
    }

    if (state->input.anyKeyDown({ weaver::Key::Tab }))
    {
      state->setEditMode(!state->editMode());
    }
    co_yield {};
  }
}
}  // namespace

int main(int argc, char *argv[])
{
  Options options{};
  if (const int error_code = parseArgs(options, argc, argv); error_code != 0)
  {
    return error_code;
  }

  auto state = std::make_shared<GlobalState>(morai::SchedulerParams{});

  state->scheduler.start(inputFibre(state), "Input");

  if (!options.edit)
  {
    state->randomise();
  }

  for (int y = 0; y < state->screen.size().height; ++y)
  {
    for (int x = 0; x < state->screen.size().width; ++x)
    {
      state->scheduler.start(conwayCellFibre(state, weaver::Coord{ x, y }),
                             std::format("{},{}", x, y));
    }
  }

  // Add a fibre to automatically start simulating.
  if (!options.edit)
  {
    state->scheduler.start(
      [](std::shared_ptr<GlobalState> state) -> morai::Fibre {
        state->setEditMode(false);
        co_return;
      }(state),
      "AutoSimulate");
  }

  // Setup render scheduler.
  state->scheduler.start(renderFibre(state), "Render");

  const double target_dt = 0.1;
  while (!state->quit)
  {
    const auto now = std::chrono::steady_clock::now();
    state->scheduler.update();
    std::this_thread::sleep_until(now + std::chrono::duration<double>(target_dt));
  }
  return 0;
}
