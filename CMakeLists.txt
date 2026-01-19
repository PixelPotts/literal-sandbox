cmake_minimum_required(VERSION 3.10)
project(SandSimulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for maximum performance
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -funroll-loops")

# Find OpenMP for parallel processing
find_package(OpenMP REQUIRED)

# Find SDL2
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

# Find SDL2_ttf
find_package(SDL2_ttf REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/Config.cpp
    src/SandSimulator.cpp
    src/World.cpp
    src/WorldChunk.cpp
    src/Sprite.cpp
    src/SceneObject.cpp
    src/Collectible.cpp
    src/Gun.cpp
    src/Bullet.cpp
    src/Texturize.cpp
    src/stb_image_impl.cpp
    src/ZLayers.cpp
    src/MainSprite.cpp
    src/LittlePurpleJumper.cpp
)

# Create executable
add_executable(sand_simulator ${SOURCES})

# Link SDL2, SDL2_ttf, SDL2_mixer, and OpenMP
target_link_libraries(sand_simulator ${SDL2_LIBRARIES} SDL2_ttf::SDL2_ttf SDL2_mixer OpenMP::OpenMP_CXX)
