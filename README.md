# Sand Simulator

A simple falling sand particle simulator written in C++ using SDL2.

## Features

- 1000x1000 pixel window
- Single-pixel sand grains falling from top center
- Realistic sand physics with diagonal falling
- Configurable rules via `rules.txt`
- Double ESC to close

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

```bash
./sand_simulator
```

## Configuration

Edit `rules.txt` to customize the simulator:

- **window_width/height**: Window dimensions
- **sand_color_r/g/b**: RGB color values for sand (0-255)
- **spawn_rate**: Number of particles spawned per frame
- **process_left_to_right**: true/false - scan direction for particle updates
- **fall_speed**: Pixels moved per update
- **diagonal_fall_chance**: Probability (0.0-1.0) for choosing left vs right when both are available

## Controls

- **Double ESC**: Exit the application
- **X button**: Close window

## Sand Physics

1. If no pixel underneath, fall straight down
2. If pixel underneath but space on bottom-left or bottom-right, fall diagonally
3. If both diagonals available, randomly choose based on `diagonal_fall_chance`
4. If only one diagonal available, fall to that side
5. If no space available, stay in place

## Project Structure

```
sand-simulator/
├── src/
│   ├── main.cpp           # Main application loop with SDL2
│   ├── Config.h/cpp       # Configuration file parser
│   └── SandSimulator.h/cpp # Sand physics simulation
├── rules.txt              # Editable configuration
├── CMakeLists.txt         # Build configuration
└── README.md             # This file
```
