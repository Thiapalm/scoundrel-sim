# Scoundrel Simulator (scoundrel-sim)

This project is a C++ automated balance simulator for the **Scoundrel** card game. It runs thousands of simulated games across different player classes to gather statistics on win rates, average scores, and common causes of defeat.

## Project Overview

- **Purpose**: Balance testing and heuristic-based automated play.
- **Tech Stack**: C++ (C++17/20), CMake.
- **Dependencies**: 
    - `scoundrel-core`: A sibling project located at `../scoundrel-core` which contains the core game logic, engine, and assets.
- **Architecture**:
    - The simulator implements the `UserInterface` from `scoundrel-core` to provide a non-interactive, heuristic-driven "AI" player.
    - `SimulationUI` handles automated decision-making for actions and card selection.
    - Statistics are collected for several player classes: `Peasant`, `Warrior`, and `Healer`.

## Building and Running

### Prerequisites
- CMake 3.16+
- A C++ compiler supporting C++17 (e.g., MSVC, GCC, Clang)
- The `scoundrel-core` source tree must be present in the parent directory.

### Build Commands
From the project root:
```powershell
# Configure CMake
cmake -B build -S .

# Build the simulator (Release mode recommended for performance)
cmake --build build --config Release
```

### Running the Simulator
The executable will be generated in `build/Release/` (on Windows/MSVC) or `build/` (on Linux).
```powershell
# Run with default iterations (1000 per class)
./build/Release/ScoundrelSimulator.exe

# Run with custom iterations
./build/Release/ScoundrelSimulator.exe 5000
```

## Project Structure

- `src/BalanceSimulator.cpp`: Main entry point and implementation of the automated player and statistics collector.
- `CMakeLists.txt`: Build configuration, links against `scoundrel-core`.
- `BUILD.md`: Detailed build instructions and dependency notes.

## Development Conventions

- **Heuristics**: If you modify the game logic in `scoundrel-core`, you may need to update the `SimulationUI` heuristics in `BalanceSimulator.cpp` to ensure the "AI" still plays effectively.
- **Adding Classes**: To test a new player class, update the `run_simulations` calls in `main()`.
- **Game Logic**: All game rules reside in `scoundrel-core`. This repository is strictly for simulation and analysis.
