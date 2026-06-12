# Scoundrel Simulator

An automated balance simulator for the **Scoundrel** card game.

## Overview

This project provides a non-interactive, heuristic-driven simulator for the Scoundrel card game. It is designed to run thousands of simulated games to gather statistics on win rates, average scores, and common causes of defeat across different player classes.

The simulator leverages the core game engine from the sibling project `scoundrel-core`.

## Features

- **Automated Play**: Implements a `SimulationUI` with heuristics to make automated decisions.
- **Class Analysis**: Supports testing for multiple player classes:
  - **Peasant**: Baseline performance.
  - **Warrior**: Focuses on combat and weapons.
  - **Healer**: High survivability through potions.
- **Statistical Output**: Detailed breakdown of win rates, scores, and failure modes.
- **Fast Execution**: Designed to run 10,000+ simulations in seconds (especially in Release mode).

## Prerequisites

- **CMake** 3.16+
- **C++ Compiler** with C++17 support (GCC, Clang, or MSVC)
- **Scoundrel Core**: The `scoundrel-core` sibling repository must be present in the parent directory.

## Building

From the project root:

```bash
# Configure the project
cmake -B build -S .

# Build the executable
cmake --build build --config Release
```

## Running

The executable is named `ScoundrelSimulator`. By default, it runs 1,000 simulations per class.

```bash
# Run with defaults
./build/ScoundrelSimulator

# Run with custom iteration count (e.g., 5000)
./build/ScoundrelSimulator 5000
```

> **Note**: On Windows with MSVC, the executable will likely be in `./build/Release/ScoundrelSimulator.exe`.

## Project Structure

- `src/BalanceSimulator.cpp`: Main simulator logic, `SimulationUI` implementation, and statistics gathering.
- `CMakeLists.txt`: Build configuration and dependency linking.
- `BUILD.md`: Detailed build and dependency notes.
- `GEMINI.md`: Internal agent instructions and project overview.

## How it Works

The simulator implements the `UserInterface` interface from `scoundrel-core`. Instead of prompting for user input, it evaluates the current game state and valid actions using a set of heuristics defined in `SimulationUI::pick_action` and `SimulationUI::pick_card`. 

This allows for high-speed simulation of game loops to identify balance issues or evaluate the impact of rule changes.
