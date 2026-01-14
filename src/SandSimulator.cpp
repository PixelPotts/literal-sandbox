#include "SandSimulator.h"
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>

SandSimulator::SandSimulator(const Config& cfg) : config(cfg), debugFrameCount(0), nextAttachmentGroupId(1),
    spawnCounter(0), currentSpawnType(ParticleType::SAND) {
    // Calculate grid size from window size and pixel scale
    width = config.windowWidth / config.pixelScale;
    height = config.windowHeight / config.pixelScale;
    grid.resize(width * height, ParticleType::EMPTY);
    colors.resize(width * height, {0, 0, 0});
    velocities.resize(width * height, {0.0f, 0.0f});
    temperature.resize(width * height, 20.0f);  // Initialize to room temperature
    wetness.resize(width * height, 0.0f);       // Initialize to dry (0.0 = no wetness)
    isSettled.resize(width * height, false);    // Start unsettled (velocity physics)
    attachmentGroup.resize(width * height, 0);
    particleAge.resize(width * height, 0);      // All particles start at age 0
    std::srand(std::time(nullptr));

    // Initialize activity tracking (performance optimization)
    chunkWidth = 16;
    chunkHeight = 16;
    chunksX = (width + chunkWidth - 1) / chunkWidth;
    chunksY = (height + chunkHeight - 1) / chunkHeight;
    chunkActivity.resize(chunksX * chunksY, {false, 0, false, 0});  // Added isSleeping and stableFrameCount
    rowHasParticles.resize(height, false);

    // Clear debug file on startup
    std::ofstream debugFile("debug.txt", std::ios::trunc);
    debugFile << "=== Particle Simulator Debug Log ===\n";
    debugFile.close();

    // Initialize particle count cache
    for (int i = 0; i < 10; ++i) {
        particleCounts[i] = 0;
    }
}

SandSimulator::~SandSimulator() {}

bool SandSimulator::inBounds(int x, int y) const {
    return x >= 0 && x < width && y >= 0 && y < height;
}

ParticleType SandSimulator::getParticleType(int x, int y) const {
    if (!inBounds(x, y)) return ParticleType::EMPTY;
    return grid[y * width + x];
}

void SandSimulator::setParticleType(int x, int y, ParticleType type) {
    if (inBounds(x, y)) {
        int idx = y * width + x;
        ParticleType oldType = grid[idx];

        // Update counters only if type changed
        if (oldType != type) {
            if (oldType != ParticleType::EMPTY) {
                decrementParticleCount(oldType);
            }
            if (type != ParticleType::EMPTY) {
                incrementParticleCount(type);
            }
        }

        grid[idx] = type;
    }
}

bool SandSimulator::isOccupied(int x, int y) const {
    return getParticleType(x, y) != ParticleType::EMPTY;
}

ParticleColor SandSimulator::getColor(int x, int y) const {
    if (!inBounds(x, y)) return {0, 0, 0};

    int idx = y * width + x;
    ParticleColor baseColor = colors[idx];

    // Apply wetness darkening for materials that can absorb water
    ParticleType type = getParticleType(x, y);
    float maxSat = getMaxSaturation(type);

    if (maxSat > 0.0f) {
        float wetnessLevel = wetness[idx];
        float wetnessRatio = wetnessLevel / maxSat;  // 0.0 to 1.0

        // Darken color based on wetness (wet sand is darker, but not black)
        // At 0% wetness: no darkening (factor = 1.0)
        // At 100% wetness: darken by 50% (factor = 0.5)
        float darkenFactor = 1.0f - (wetnessRatio * 0.5f);

        ParticleColor darkenedColor;
        darkenedColor.r = static_cast<unsigned char>(baseColor.r * darkenFactor);
        darkenedColor.g = static_cast<unsigned char>(baseColor.g * darkenFactor);
        darkenedColor.b = static_cast<unsigned char>(baseColor.b * darkenFactor);

        return darkenedColor;
    }

    return baseColor;
}

int SandSimulator::getParticleCount(ParticleType type) const {
    return particleCounts[static_cast<int>(type)];
}

void SandSimulator::incrementParticleCount(ParticleType type) {
    particleCounts[static_cast<int>(type)]++;
}

void SandSimulator::decrementParticleCount(ParticleType type) {
    particleCounts[static_cast<int>(type)]--;
}

int SandSimulator::clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

bool SandSimulator::shouldUpdateMovement(ParticleType type) const {
    // Always allow vertical falling through empty space
    // This is just for compatibility - shouldn't be used in main loop
    return true;
}

bool SandSimulator::shouldUpdateHorizontalMovement(ParticleType type) const {
    // Viscosity affects horizontal flow and displacement, not vertical falling
    int frequency = 1;
    switch (type) {
        case ParticleType::SAND: frequency = config.sand.movementFrequency; break;
        case ParticleType::WATER: frequency = config.water.movementFrequency; break;
        case ParticleType::ROCK: frequency = config.rock.movementFrequency; break;
        case ParticleType::LAVA: frequency = config.lava.movementFrequency; break;
        case ParticleType::STEAM: frequency = config.steam.movementFrequency; break;
        case ParticleType::OBSIDIAN: frequency = config.obsidian.movementFrequency; break;
        case ParticleType::FIRE: frequency = config.fire.movementFrequency; break;
        case ParticleType::ICE: frequency = config.ice.movementFrequency; break;
        case ParticleType::GLASS: frequency = config.glass.movementFrequency; break;
        default: frequency = 1; break;
    }

    if (frequency <= 1) return true;
    return (debugFrameCount % frequency) == 0;
}

// Hybrid physics helpers - determine if particle should use cellular or velocity physics
bool SandSimulator::shouldBeSettled(int x, int y) const {
    int idx = y * width + x;
    ParticleType type = getParticleType(x, y);

    if (type == ParticleType::EMPTY) return false;

    // Steam and fire are always "settled" = use cellular automaton physics (which makes them rise/move)
    // (settled doesn't mean "not moving", it means "use cheap cellular physics instead of velocity")
    if (type == ParticleType::STEAM || type == ParticleType::FIRE) return true;

    // Rock, wood, and obsidian are always settled (they don't move much)
    if (type == ParticleType::ROCK || type == ParticleType::OBSIDIAN || type == ParticleType::WOOD) return true;

    // Check velocity - if moving fast, stay unsettled
    float vx = velocities[idx].vx;
    float vy = velocities[idx].vy;
    float speed = std::sqrt(vx * vx + vy * vy);

    if (speed > 0.5f) return false;  // Moving fast = unsettled

    // Count neighbors (particles in contact)
    int neighborCount = 0;
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int dir = 0; dir < 8; ++dir) {
        int nx = x + dx[dir];
        int ny = y + dy[dir];
        if (inBounds(nx, ny) && getParticleType(nx, ny) != ParticleType::EMPTY) {
            neighborCount++;
        }
    }

    // If touching 3+ neighbors and moving slowly = settled
    if (neighborCount >= 3) return true;

    // Check if particle is resting on something below
    if (y + 1 < height) {
        ParticleType below = getParticleType(x, y + 1);
        if (below != ParticleType::EMPTY && speed < 0.1f) {
            // Resting on ground, has neighbors, moving very slowly = settled
            if (neighborCount >= 2) return true;
        }
    }

    // Default: unsettled (use velocity physics)
    return false;
}

void SandSimulator::updateSettledState(int x, int y) {
    int idx = y * width + x;
    ParticleType type = getParticleType(x, y);

    if (type == ParticleType::EMPTY) {
        isSettled[idx] = false;
        return;
    }

    bool shouldSettle = shouldBeSettled(x, y);

    // Transition: unsettled -> settled
    if (!isSettled[idx] && shouldSettle) {
        isSettled[idx] = true;
        // Snap velocity to zero when settling
        velocities[idx] = {0.0f, 0.0f};
    }
    // Transition: settled -> unsettled
    else if (isSettled[idx] && !shouldSettle) {
        isSettled[idx] = false;
        // Keep velocity (might have been disturbed by neighbor)
    }
}

// Activity tracking helpers for performance optimization
int SandSimulator::getChunkIndex(int chunkX, int chunkY) const {
    return chunkY * chunksX + chunkX;
}

bool SandSimulator::isChunkActive(int chunkX, int chunkY) const {
    if (chunkX < 0 || chunkX >= chunksX || chunkY < 0 || chunkY >= chunksY) return false;
    return chunkActivity[getChunkIndex(chunkX, chunkY)].isActive;
}

void SandSimulator::activateChunk(int chunkX, int chunkY) {
    if (chunkX < 0 || chunkX >= chunksX || chunkY < 0 || chunkY >= chunksY) return;
    int idx = getChunkIndex(chunkX, chunkY);
    chunkActivity[idx].isActive = true;
    chunkActivity[idx].lastActiveFrame = debugFrameCount;
}

void SandSimulator::activateChunkAtPosition(int x, int y) {
    int chunkX = x / chunkWidth;
    int chunkY = y / chunkHeight;
    activateChunk(chunkX, chunkY);
}

bool SandSimulator::chunkHasParticles(int chunkX, int chunkY) const {
    int startX = chunkX * chunkWidth;
    int startY = chunkY * chunkHeight;
    int endX = std::min(startX + chunkWidth, width);
    int endY = std::min(startY + chunkHeight, height);

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            if (getParticleType(x, y) != ParticleType::EMPTY) {
                return true;
            }
        }
    }
    return false;
}

bool SandSimulator::anyNeighborChunkActive(int chunkX, int chunkY) const {
    // Check all 8 neighboring chunks
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;  // Skip self
            if (isChunkActive(chunkX + dx, chunkY + dy)) {
                return true;
            }
        }
    }
    return false;
}

void SandSimulator::updateChunkActivity() {
    // Update activity for all chunks
    for (int cy = 0; cy < chunksY; ++cy) {
        for (int cx = 0; cx < chunksX; ++cx) {
            int idx = getChunkIndex(cx, cy);

            // Chunks with particles only wake when explicitly woken by particle movement
            // (handled by wakeChunkAtPosition in moveParticle/swapParticles)

            // Activate if has particles
            if (chunkHasParticles(cx, cy)) {
                chunkActivity[idx].isActive = true;
                chunkActivity[idx].lastActiveFrame = debugFrameCount;

                // Check if chunk is stable (no movement)
                if (isChunkStable(cx, cy)) {
                    chunkActivity[idx].stableFrameCount++;

                    // Put chunk to sleep after being stable for threshold frames
                    if (chunkActivity[idx].stableFrameCount >= FRAMES_UNTIL_SLEEP) {
                        chunkActivity[idx].isSleeping = true;
                    }
                } else {
                    // Chunk has movement, reset stable counter and wake up
                    chunkActivity[idx].stableFrameCount = 0;
                    chunkActivity[idx].isSleeping = false;
                }
            }
            // Deactivate after 10 frames of emptiness
            else if (debugFrameCount - chunkActivity[idx].lastActiveFrame > 10) {
                chunkActivity[idx].isActive = false;
                chunkActivity[idx].isSleeping = true;  // Empty chunks are sleeping
            }
            // Keep active if neighbors are active (particles might enter)
            else if (anyNeighborChunkActive(cx, cy)) {
                chunkActivity[idx].isActive = true;
            }
        }
    }
}

void SandSimulator::buildRowSkipList() {
    // Build list of rows that have particles (skip empty rows)
    for (int y = 0; y < height; ++y) {
        rowHasParticles[y] = false;
        for (int x = 0; x < width; ++x) {
            if (getParticleType(x, y) != ParticleType::EMPTY) {
                rowHasParticles[y] = true;
                break;
            }
        }
    }
}

// Sleep tracking helpers
bool SandSimulator::isChunkSleeping(int chunkX, int chunkY) const {
    if (chunkX < 0 || chunkX >= chunksX || chunkY < 0 || chunkY >= chunksY) return false;
    return chunkActivity[getChunkIndex(chunkX, chunkY)].isSleeping;
}

// Debug visualization methods
bool SandSimulator::isChunkActiveForDebug(int chunkX, int chunkY) const {
    if (chunkX < 0 || chunkX >= chunksX || chunkY < 0 || chunkY >= chunksY) return false;
    return chunkActivity[getChunkIndex(chunkX, chunkY)].isActive;
}

bool SandSimulator::isChunkSleepingForDebug(int chunkX, int chunkY) const {
    return isChunkSleeping(chunkX, chunkY);
}

void SandSimulator::wakeChunk(int chunkX, int chunkY) {
    if (chunkX < 0 || chunkX >= chunksX || chunkY < 0 || chunkY >= chunksY) return;
    int idx = getChunkIndex(chunkX, chunkY);
    chunkActivity[idx].isSleeping = false;
    chunkActivity[idx].stableFrameCount = 0;
    chunkActivity[idx].isActive = true;
    chunkActivity[idx].lastActiveFrame = debugFrameCount;
}

void SandSimulator::wakeChunkAtPosition(int x, int y) {
    int chunkX = x / chunkWidth;
    int chunkY = y / chunkHeight;
    wakeChunk(chunkX, chunkY);
}

void SandSimulator::wakeNeighborChunks(int chunkX, int chunkY) {
    // Wake all 8 neighboring chunks
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            wakeChunk(chunkX + dx, chunkY + dy);
        }
    }
}

bool SandSimulator::isChunkStable(int chunkX, int chunkY) const {
    // Check if all particles in chunk are settled (not moving)
    int startX = chunkX * chunkWidth;
    int startY = chunkY * chunkHeight;
    int endX = std::min(startX + chunkWidth, width);
    int endY = std::min(startY + chunkHeight, height);

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            int idx = y * width + x;
            ParticleType type = getParticleType(x, y);

            if (type != ParticleType::EMPTY) {
                // Steam and fire always keep chunks unstable (they're always moving)
                if (type == ParticleType::STEAM || type == ParticleType::FIRE) {
                    return false;
                }

                // Check if particle is settled
                if (!isSettled[idx]) {
                    return false;  // Has unsettled particle = not stable
                }

                // Also check if it has significant velocity
                if (std::abs(velocities[idx].vx) > 0.01f || std::abs(velocities[idx].vy) > 0.01f) {
                    return false;  // Has velocity = not stable
                }
            }
        }
    }
    return true;  // All particles settled or empty = stable
}

void SandSimulator::logDebug(const std::string& message) {
    if (debugFrameCount > 10) return; // Only log first 10 frames
    std::ofstream debugFile("debug.txt", std::ios::app);
    debugFile << message << "\n";
    debugFile.close();
}

ParticleColor SandSimulator::generateRandomColor(int baseR, int baseG, int baseB, int variation) {
    ParticleColor color;
    if (variation > 0) {
        color.r = clamp(baseR + (std::rand() % (variation * 2 + 1)) - variation, 0, 255);
        color.g = clamp(baseG + (std::rand() % (variation * 2 + 1)) - variation, 0, 255);
        color.b = clamp(baseB + (std::rand() % (variation * 2 + 1)) - variation, 0, 255);
    } else {
        color.r = baseR;
        color.g = baseG;
        color.b = baseB;
    }
    return color;
}

void SandSimulator::moveParticle(int fromX, int fromY, int toX, int toY) {
    // Validate destination is in bounds before moving
    if (!inBounds(toX, toY)) return;

    // Wake destination chunk and source chunk (movement happening!)
    wakeChunkAtPosition(toX, toY);
    wakeChunkAtPosition(fromX, fromY);

    ParticleType type = getParticleType(fromX, fromY);
    ParticleColor color = colors[fromY * width + fromX];
    ParticleVelocity velocity = velocities[fromY * width + fromX];
    float temp = temperature[fromY * width + fromX];
    float wet = wetness[fromY * width + fromX];
    bool settled = isSettled[fromY * width + fromX];
    int group = attachmentGroup[fromY * width + fromX];
    int age = particleAge[fromY * width + fromX];

    setParticleType(fromX, fromY, ParticleType::EMPTY);
    setParticleType(toX, toY, type);
    colors[toY * width + toX] = color;
    velocities[toY * width + toX] = velocity;
    temperature[toY * width + toX] = temp;
    wetness[toY * width + toX] = wet;
    isSettled[toY * width + toX] = settled;
    attachmentGroup[toY * width + toX] = group;
    particleAge[toY * width + toX] = age;
    velocities[fromY * width + fromX] = {0.0f, 0.0f};
    temperature[fromY * width + fromX] = 20.0f;  // Reset to ambient
    wetness[fromY * width + fromX] = 0.0f;       // Reset to dry
    isSettled[fromY * width + fromX] = false;    // Reset to unsettled
    attachmentGroup[fromY * width + fromX] = 0;
    particleAge[fromY * width + fromX] = 0;
}

void SandSimulator::swapParticles(int x1, int y1, int x2, int y2) {
    if (!inBounds(x1, y1) || !inBounds(x2, y2)) return;

    // Wake both chunks involved in the swap
    wakeChunkAtPosition(x1, y1);
    wakeChunkAtPosition(x2, y2);

    int idx1 = y1 * width + x1;
    int idx2 = y2 * width + x2;

    // Swap types
    ParticleType temp_type = grid[idx1];
    grid[idx1] = grid[idx2];
    grid[idx2] = temp_type;

    // Swap colors
    ParticleColor temp_color = colors[idx1];
    colors[idx1] = colors[idx2];
    colors[idx2] = temp_color;

    // Swap velocities
    ParticleVelocity temp_vel = velocities[idx1];
    velocities[idx1] = velocities[idx2];
    velocities[idx2] = temp_vel;

    // Swap temperatures
    float temp_temp = temperature[idx1];
    temperature[idx1] = temperature[idx2];
    temperature[idx2] = temp_temp;

    // Swap wetness
    float temp_wet = wetness[idx1];
    wetness[idx1] = wetness[idx2];
    wetness[idx2] = temp_wet;

    // Swap settled state
    bool temp_settled = isSettled[idx1];
    isSettled[idx1] = isSettled[idx2];
    isSettled[idx2] = temp_settled;

    // Swap attachment groups
    int temp_group = attachmentGroup[idx1];
    attachmentGroup[idx1] = attachmentGroup[idx2];
    attachmentGroup[idx2] = temp_group;

    // Swap ages
    int temp_age = particleAge[idx1];
    particleAge[idx1] = particleAge[idx2];
    particleAge[idx2] = temp_age;
}

void SandSimulator::spawnParticleAt(int x, int y, ParticleType type) {
    if (!inBounds(x, y)) return;

    // Wake chunk and neighbors when spawning particles (user interaction)
    wakeChunkAtPosition(x, y);
    int chunkX = x / chunkWidth;
    int chunkY = y / chunkHeight;
    wakeNeighborChunks(chunkX, chunkY);

    setParticleType(x, y, type);

    if (type == ParticleType::SAND) {
        colors[y * width + x] = generateRandomColor(config.sand.colorR, config.sand.colorG, config.sand.colorB, config.sand.colorVariation);
    } else if (type == ParticleType::WATER) {
        colors[y * width + x] = generateRandomColor(config.water.colorR, config.water.colorG, config.water.colorB, config.water.colorVariation);
    } else if (type == ParticleType::ROCK) {
        colors[y * width + x] = generateRandomColor(config.rock.colorR, config.rock.colorG, config.rock.colorB, config.rock.colorVariation);
    } else if (type == ParticleType::LAVA) {
        colors[y * width + x] = generateRandomColor(config.lava.colorR, config.lava.colorG, config.lava.colorB, config.lava.colorVariation);
    } else if (type == ParticleType::STEAM) {
        colors[y * width + x] = generateRandomColor(config.steam.colorR, config.steam.colorG, config.steam.colorB, config.steam.colorVariation);
    } else if (type == ParticleType::OBSIDIAN) {
        colors[y * width + x] = generateRandomColor(config.obsidian.colorR, config.obsidian.colorG, config.obsidian.colorB, config.obsidian.colorVariation);
    } else if (type == ParticleType::FIRE) {
        colors[y * width + x] = generateRandomColor(config.fire.colorR, config.fire.colorG, config.fire.colorB, config.fire.colorVariation);
    } else if (type == ParticleType::ICE) {
        colors[y * width + x] = generateRandomColor(config.ice.colorR, config.ice.colorG, config.ice.colorB, config.ice.colorVariation);
    } else if (type == ParticleType::GLASS) {
        colors[y * width + x] = generateRandomColor(config.glass.colorR, config.glass.colorG, config.glass.colorB, config.glass.colorVariation);
    } else if (type == ParticleType::WOOD) {
        colors[y * width + x] = generateRandomColor(config.wood.colorR, config.wood.colorG, config.wood.colorB, config.wood.colorVariation);
    }

    velocities[y * width + x] = {0.0f, 0.0f};
    temperature[y * width + x] = getBaseTemperature(type);
    attachmentGroup[y * width + x] = 0; // Single particles not attached
    particleAge[y * width + x] = 0;      // Start at age 0

    // Initialize wetness - water particles start with 1.0 (one unit of water)
    if (type == ParticleType::WATER) {
        wetness[y * width + x] = 1.0f;
    } else {
        wetness[y * width + x] = 0.0f;
    }

    // Start all particles as unsettled (will settle when conditions are met)
    isSettled[y * width + x] = false;

    // Activate chunk for new particle (performance optimization)
    activateChunkAtPosition(x, y);
}

void SandSimulator::spawnRockCluster(int centerX, int centerY) {
    if (!inBounds(centerX, centerY)) return;

    int groupId = nextAttachmentGroupId++;
    int radius = 2 + (std::rand() % 2); // Random radius 2-3

    // Track positions for cache
    std::vector<std::pair<int, int>> groupPositions;

    // Spawn circular cluster
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int dist = dx * dx + dy * dy;
            if (dist <= radius * radius) { // Inside circle
                int x = centerX + dx;
                int y = centerY + dy;

                if (inBounds(x, y)) {
                    setParticleType(x, y, ParticleType::ROCK);
                    colors[y * width + x] = generateRandomColor(config.rock.colorR, config.rock.colorG, config.rock.colorB, config.rock.colorVariation);
                    velocities[y * width + x] = {0.0f, 0.0f};
                    attachmentGroup[y * width + x] = groupId;
                    particleAge[y * width + x] = 0;
                    groupPositions.push_back({x, y});
                }
            }
        }
    }

    // Populate cache with this group's positions
    rockGroupCache[groupId] = groupPositions;
}

void SandSimulator::spawnWoodCluster(int centerX, int centerY) {
    if (!inBounds(centerX, centerY)) return;

    int groupId = nextAttachmentGroupId++;
    int radius = 2 + (std::rand() % 2); // Random radius 2-3

    // Track positions for cache
    std::vector<std::pair<int, int>> groupPositions;

    // Spawn circular cluster
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int dist = dx * dx + dy * dy;
            if (dist <= radius * radius) { // Inside circle
                int x = centerX + dx;
                int y = centerY + dy;

                if (inBounds(x, y)) {
                    setParticleType(x, y, ParticleType::WOOD);
                    colors[y * width + x] = generateRandomColor(config.wood.colorR, config.wood.colorG, config.wood.colorB, config.wood.colorVariation);
                    velocities[y * width + x] = {0.0f, 0.0f};
                    attachmentGroup[y * width + x] = groupId;
                    particleAge[y * width + x] = 0;
                    groupPositions.push_back({x, y});
                }
            }
        }
    }

    // Populate cache with this group's positions
    rockGroupCache[groupId] = groupPositions;
}

void SandSimulator::spawnParticles() {
    // Sequential spawning: 300 sand, 600 water, 100 lava
    // Check if we're done spawning
    if (spawnCounter >= 1000) return; // 300 + 600 + 100 = 1000

    // Determine what to spawn based on count
    ParticleType typeToSpawn;
    ParticleTypeConfig* spawnConfig;

    if (spawnCounter < 300) {
        typeToSpawn = ParticleType::SAND;
        spawnConfig = &config.sand;
    } else if (spawnCounter < 900) {  // 300 + 600
        typeToSpawn = ParticleType::WATER;
        spawnConfig = &config.water;
    } else {  // 900 to 1000
        typeToSpawn = ParticleType::LAVA;
        spawnConfig = &config.lava;
    }

    // Spawn one particle per call
    int baseX = width / 2;  // Center
    switch (spawnConfig->spawnPosition) {
        case SpawnPosition::LEFT: baseX = 0; break;
        case SpawnPosition::RIGHT: baseX = width - 1; break;
        case SpawnPosition::CENTER: baseX = width / 2; break;
    }

    int spawnX = baseX;
    if (spawnConfig->spawnPositionRandomness > 0) {
        int offset = (std::rand() % (spawnConfig->spawnPositionRandomness * 2 + 1)) - spawnConfig->spawnPositionRandomness;
        spawnX = clamp(baseX + offset, 0, width - 1);
    }

    spawnParticleAt(spawnX, 0, typeToSpawn);
    spawnCounter++;
}

// Sand Physics - Basic falling with diagonals
void SandSimulator::updateSandParticle(int x, int y) {
    if (y + 1 >= height) return;

    ParticleType myType = getParticleType(x, y);

    // Rule 1: Fall straight down (or displace lighter particle)
    if (!isOccupied(x, y + 1)) {
        moveParticle(x, y, x, y + 1);
        return;
    } else if (canDisplace(myType, getParticleType(x, y + 1))) {
        // Swap with lighter particle below
        swapParticles(x, y, x, y + 1);
        return;
    }

    // Rule 2: Fall diagonally (or displace lighter particle)
    bool leftOpen = (x - 1 >= 0 && y + 1 < height && !isOccupied(x - 1, y + 1));
    bool rightOpen = (x + 1 < width && y + 1 < height && !isOccupied(x + 1, y + 1));
    bool leftDisplace = (x - 1 >= 0 && y + 1 < height && canDisplace(myType, getParticleType(x - 1, y + 1)));
    bool rightDisplace = (x + 1 < width && y + 1 < height && canDisplace(myType, getParticleType(x + 1, y + 1)));

    if (leftOpen && rightOpen) {
        float random = static_cast<float>(std::rand()) / RAND_MAX;
        int newX = (random < config.sand.diagonalFallChance) ? x - 1 : x + 1;
        moveParticle(x, y, newX, y + 1);
        return;
    } else if (leftOpen) {
        moveParticle(x, y, x - 1, y + 1);
        return;
    } else if (rightOpen) {
        moveParticle(x, y, x + 1, y + 1);
        return;
    } else if (leftDisplace && rightDisplace) {
        float random = static_cast<float>(std::rand()) / RAND_MAX;
        int newX = (random < config.sand.diagonalFallChance) ? x - 1 : x + 1;
        // Swap with lighter particle
        swapParticles(x, y, newX, y + 1);
        return;
    } else if (leftDisplace) {
        // Swap with lighter particle on left diagonal
        swapParticles(x, y, x - 1, y + 1);
        return;
    } else if (rightDisplace) {
        // Swap with lighter particle on right diagonal
        swapParticles(x, y, x + 1, y + 1);
        return;
    }

    // Rule 3: Try slope sliding (settled particles on steep slopes)
    if (trySandSlopeSlide(x, y)) return;

    // Rule 4: Try horizontal spreading (on flat surfaces)
    if (trySandHorizontalSpread(x, y)) return;

    // Rule 5: Random tumbling (settled particles occasionally shift)
    if (trySandRandomTumble(x, y)) return;
}

// Slope sliding: If particle is settled but steep angle, slide down
bool SandSimulator::trySandSlopeSlide(int x, int y) {
    int dist = config.sand.slopeSlideDistance;
    if (dist <= 0) return false;

    // Check if we're on a steep left slope
    if (x - 1 >= 0 && y + dist < height) {
        bool canSlideLeft = true;
        // Check if diagonal path is clear
        for (int i = 1; i <= dist; ++i) {
            if (isOccupied(x - 1, y + i)) {
                canSlideLeft = false;
                break;
            }
        }
        if (canSlideLeft && !isOccupied(x - 1, y + 1)) {
            moveParticle(x, y, x - 1, y + 1);
            return true;
        }
    }

    // Check if we're on a steep right slope
    if (x + 1 < width && y + dist < height) {
        bool canSlideRight = true;
        for (int i = 1; i <= dist; ++i) {
            if (isOccupied(x + 1, y + i)) {
                canSlideRight = false;
                break;
            }
        }
        if (canSlideRight && !isOccupied(x + 1, y + 1)) {
            moveParticle(x, y, x + 1, y + 1);
            return true;
        }
    }

    return false;
}

// Horizontal spreading: Particles on flat surfaces spread sideways
bool SandSimulator::trySandHorizontalSpread(int x, int y) {
    int dist = config.sand.horizontalSpreadDistance;
    if (dist <= 0) return false;

    // Only spread if we're settled (something below us)
    if (!isOccupied(x, y + 1)) return false;

    // Try spreading left
    if (x - dist >= 0 && !isOccupied(x - dist, y)) {
        // Check if there's a gap that needs filling
        bool foundGap = false;
        for (int checkDist = 1; checkDist <= dist; ++checkDist) {
            int checkX = x - checkDist;
            if (!isOccupied(checkX, y + 1)) {
                foundGap = true;
                break;
            }
        }
        if (foundGap && !isOccupied(x - 1, y)) {
            moveParticle(x, y, x - 1, y);
            return true;
        }
    }

    // Try spreading right
    if (x + dist < width && !isOccupied(x + dist, y)) {
        bool foundGap = false;
        for (int checkDist = 1; checkDist <= dist; ++checkDist) {
            int checkX = x + checkDist;
            if (!isOccupied(checkX, y + 1)) {
                foundGap = true;
                break;
            }
        }
        if (foundGap && !isOccupied(x + 1, y)) {
            moveParticle(x, y, x + 1, y);
            return true;
        }
    }

    return false;
}

// Random tumbling: Settled particles occasionally shift randomly
bool SandSimulator::trySandRandomTumble(int x, int y) {
    float chance = config.sand.randomTumbleChance;
    if (chance <= 0.0f) return false;

    float random = static_cast<float>(std::rand()) / RAND_MAX;
    if (random > chance) return false;

    // Try tumbling left or right
    bool leftClear = !isOccupied(x - 1, y);
    bool rightClear = !isOccupied(x + 1, y);

    if (leftClear && rightClear) {
        int dir = (std::rand() % 2) ? -1 : 1;
        moveParticle(x, y, x + dir, y);
        return true;
    } else if (leftClear) {
        moveParticle(x, y, x - 1, y);
        return true;
    } else if (rightClear) {
        moveParticle(x, y, x + 1, y);
        return true;
    }

    return false;
}

// Water Physics - Flows like liquid
void SandSimulator::updateWaterParticle(int x, int y) {
    if (y + 1 >= height) return;

    ParticleType myType = getParticleType(x, y);

    // Rule 1: Fall straight down (or displace lighter particle)
    if (!isOccupied(x, y + 1)) {
        moveParticle(x, y, x, y + 1);
        return;
    } else if (canDisplace(myType, getParticleType(x, y + 1))) {
        // Swap with lighter particle below
        swapParticles(x, y, x, y + 1);
        return;
    }

    // Rule 2: Fall diagonally (or displace lighter particle)
    bool leftDiagOpen = (x - 1 >= 0 && y + 1 < height && !isOccupied(x - 1, y + 1));
    bool rightDiagOpen = (x + 1 < width && y + 1 < height && !isOccupied(x + 1, y + 1));
    bool leftDisplace = (x - 1 >= 0 && y + 1 < height && canDisplace(myType, getParticleType(x - 1, y + 1)));
    bool rightDisplace = (x + 1 < width && y + 1 < height && canDisplace(myType, getParticleType(x + 1, y + 1)));

    if (leftDiagOpen && rightDiagOpen) {
        float random = static_cast<float>(std::rand()) / RAND_MAX;
        int newX = (random < 0.5f) ? x - 1 : x + 1;
        moveParticle(x, y, newX, y + 1);
        return;
    } else if (leftDiagOpen) {
        moveParticle(x, y, x - 1, y + 1);
        return;
    } else if (rightDiagOpen) {
        moveParticle(x, y, x + 1, y + 1);
        return;
    } else if (leftDisplace && rightDisplace) {
        float random = static_cast<float>(std::rand()) / RAND_MAX;
        int newX = (random < 0.5f) ? x - 1 : x + 1;
        // Swap with lighter particle
        swapParticles(x, y, newX, y + 1);
        return;
    } else if (leftDisplace) {
        // Swap with lighter particle on left diagonal
        swapParticles(x, y, x - 1, y + 1);
        return;
    } else if (rightDisplace) {
        // Swap with lighter particle on right diagonal
        swapParticles(x, y, x + 1, y + 1);
        return;
    }

    // Rule 3: Flow horizontally (water spreads fast, affected by viscosity)
    if (config.water.horizontalFlowSpeed > 0 && shouldUpdateHorizontalMovement(myType)) {
        int flowSpeed = config.water.horizontalFlowSpeed;
        for (int speed = flowSpeed; speed >= 1; --speed) {
            bool leftOpen = (x - speed >= 0) && !isOccupied(x - speed, y);
            bool rightOpen = (x + speed < width) && !isOccupied(x + speed, y);

            if (leftOpen && rightOpen) {
                float random = static_cast<float>(std::rand()) / RAND_MAX;
                int newX = (random < config.water.waterDispersionChance) ? x - speed : x + speed;
                moveParticle(x, y, newX, y);
                return;
            } else if (leftOpen) {
                moveParticle(x, y, x - speed, y);
                return;
            } else if (rightOpen) {
                moveParticle(x, y, x + speed, y);
                return;
            }
        }
    }
}

void SandSimulator::updateRockParticle(int x, int y) {
    if (y + 1 >= height) return;

    int groupId = attachmentGroup[y * width + x];
    if (groupId == 0) {
        // Single rock particle - acts like heavy sand
        ParticleType myType = getParticleType(x, y);
        if (!isOccupied(x, y + 1)) {
            moveParticle(x, y, x, y + 1);
            return;
        } else if (canDisplace(myType, getParticleType(x, y + 1))) {
            // Swap with lighter particle
            swapParticles(x, y, x, y + 1);
        }
        return;
    }

    // For attached groups, check if entire group can move down
    // Only process each group once per frame (first rock to be evaluated)
    // Use mutex to protect shared set access in parallel region
    {
        std::lock_guard<std::mutex> lock(rockGroupMutex);
        if (processedRockGroupsThisFrame.find(groupId) != processedRockGroupsThisFrame.end()) {
            // This group was already processed this frame, skip
            return;
        }
        processedRockGroupsThisFrame.insert(groupId);
    }

    // Use cached group particles instead of scanning entire grid
    auto cacheIt = rockGroupCache.find(groupId);
    if (cacheIt == rockGroupCache.end()) {
        // Cache miss - this shouldn't happen, but handle gracefully
        return;
    }
    std::vector<std::pair<int, int>> groupParticles = cacheIt->second; // Copy for sorting

    // Check if all group particles can move down (or displace)
    bool canMoveDown = true;
    for (const auto& [px, py] : groupParticles) {
        if (py + 1 >= height) {
            canMoveDown = false;
            break;
        }

        int belowIdx = (py + 1) * width + px;
        if (attachmentGroup[belowIdx] == groupId) {
            // Another rock in same group below, that's fine
            continue;
        }

        if (!isOccupied(px, py + 1)) {
            // Empty below, good
            continue;
        }

        if (!canDisplace(ParticleType::ROCK, getParticleType(px, py + 1))) {
            // Can't displace what's below
            canMoveDown = false;
            break;
        }
    }

    if (canMoveDown) {
        // Move entire group down (process from bottom to top to avoid overwriting)
        std::sort(groupParticles.begin(), groupParticles.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        std::vector<std::pair<int, int>> newPositions;
        newPositions.reserve(groupParticles.size());

        for (const auto& [px, py] : groupParticles) {
            int belowIdx = (py + 1) * width + px;
            if (attachmentGroup[belowIdx] != groupId) {
                moveParticle(px, py, px, py + 1);
                attachmentGroup[belowIdx] = groupId;
                attachmentGroup[py * width + px] = 0;
                newPositions.push_back({px, py + 1});
            } else {
                newPositions.push_back({px, py});
            }
        }

        // Update cache with new positions
        rockGroupCache[groupId] = newPositions;
    }
}

void SandSimulator::updateWoodParticle(int x, int y) {
    if (y + 1 >= height) return;

    int groupId = attachmentGroup[y * width + x];
    if (groupId == 0) {
        // Single wood particle - acts like heavy sand
        ParticleType myType = getParticleType(x, y);
        if (!isOccupied(x, y + 1)) {
            moveParticle(x, y, x, y + 1);
            return;
        } else if (canDisplace(myType, getParticleType(x, y + 1))) {
            // Swap with lighter particle
            swapParticles(x, y, x, y + 1);
        }
        return;
    }

    // For attached groups, check if entire group can move down
    // Only process each group once per frame (first wood to be evaluated)
    // Use mutex to protect shared set access in parallel region
    {
        std::lock_guard<std::mutex> lock(rockGroupMutex);
        if (processedRockGroupsThisFrame.find(groupId) != processedRockGroupsThisFrame.end()) {
            // This group was already processed this frame, skip
            return;
        }
        processedRockGroupsThisFrame.insert(groupId);
    }

    // Use cached group particles instead of scanning entire grid
    auto cacheIt = rockGroupCache.find(groupId);
    if (cacheIt == rockGroupCache.end()) {
        // Cache miss - this shouldn't happen, but handle gracefully
        return;
    }
    std::vector<std::pair<int, int>> groupParticles = cacheIt->second; // Copy for sorting

    // Check if all group particles can move down (or displace)
    bool canMoveDown = true;
    for (const auto& [px, py] : groupParticles) {
        if (py + 1 >= height) {
            canMoveDown = false;
            break;
        }

        int belowIdx = (py + 1) * width + px;
        if (attachmentGroup[belowIdx] == groupId) {
            // Another wood in same group below, that's fine
            continue;
        }

        if (!isOccupied(px, py + 1)) {
            // Empty below, good
            continue;
        }

        if (!canDisplace(ParticleType::WOOD, getParticleType(px, py + 1))) {
            // Can't displace what's below
            canMoveDown = false;
            break;
        }
    }

    if (canMoveDown) {
        // Move entire group down (process from bottom to top to avoid overwriting)
        std::sort(groupParticles.begin(), groupParticles.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        std::vector<std::pair<int, int>> newPositions;
        newPositions.reserve(groupParticles.size());

        for (const auto& [px, py] : groupParticles) {
            int belowIdx = (py + 1) * width + px;
            if (attachmentGroup[belowIdx] != groupId) {
                moveParticle(px, py, px, py + 1);
                attachmentGroup[belowIdx] = groupId;
                attachmentGroup[py * width + px] = 0;
                newPositions.push_back({px, py + 1});
            } else {
                newPositions.push_back({px, py});
            }
        }

        // Update cache with new positions
        rockGroupCache[groupId] = newPositions;
    }
}

void SandSimulator::updateLavaParticle(int x, int y) {
    // Temperature-based interactions handled by heat transfer system
    // Lava falls like sand (similar physics)
    if (y + 1 >= height) return;

    ParticleType myType = getParticleType(x, y);

    // Fall straight down (or displace lighter particle)
    if (!isOccupied(x, y + 1)) {
        moveParticle(x, y, x, y + 1);
        return;
    } else if (canDisplace(myType, getParticleType(x, y + 1))) {
        swapParticles(x, y, x, y + 1);
        return;
    }

    // Fall diagonally (or displace lighter particle)
    bool leftOpen = (x - 1 >= 0 && y + 1 < height && !isOccupied(x - 1, y + 1));
    bool rightOpen = (x + 1 < width && y + 1 < height && !isOccupied(x + 1, y + 1));
    bool leftDisplace = (x - 1 >= 0 && y + 1 < height && canDisplace(myType, getParticleType(x - 1, y + 1)));
    bool rightDisplace = (x + 1 < width && y + 1 < height && canDisplace(myType, getParticleType(x + 1, y + 1)));

    if (leftOpen && rightOpen) {
        float random = static_cast<float>(std::rand()) / RAND_MAX;
        int newX = (random < config.lava.diagonalFallChance) ? x - 1 : x + 1;
        moveParticle(x, y, newX, y + 1);
        return;
    } else if (leftOpen) {
        moveParticle(x, y, x - 1, y + 1);
        return;
    } else if (rightOpen) {
        moveParticle(x, y, x + 1, y + 1);
        return;
    } else if (leftDisplace) {
        swapParticles(x, y, x - 1, y + 1);
        return;
    } else if (rightDisplace) {
        swapParticles(x, y, x + 1, y + 1);
        return;
    }

    // Lava flows horizontally like water (affected by viscosity)
    if (config.lava.horizontalFlowSpeed > 0 && shouldUpdateHorizontalMovement(myType)) {
        for (int i = 1; i <= config.lava.horizontalFlowSpeed; ++i) {
            bool leftFree = (x - i >= 0 && !isOccupied(x - i, y));
            bool rightFree = (x + i < width && !isOccupied(x + i, y));

            if (leftFree && rightFree) {
                float random = static_cast<float>(std::rand()) / RAND_MAX;
                int newX = (random < config.lava.waterDispersionChance) ? x - i : x + i;
                moveParticle(x, y, newX, y);
                return;
            } else if (leftFree) {
                moveParticle(x, y, x - i, y);
                return;
            } else if (rightFree) {
                moveParticle(x, y, x + i, y);
                return;
            }
        }
    }
}

void SandSimulator::updateSteamParticle(int x, int y) {
    // Steam rises (negative mass - go UP instead of DOWN)
    if (y - 1 < 0) return;

    ParticleType myType = getParticleType(x, y);

    // Rise straight up (or displace heavier particle due to negative mass)
    if (!isOccupied(x, y - 1)) {
        moveParticle(x, y, x, y - 1);
        return;
    } else if (canDisplace(myType, getParticleType(x, y - 1))) {
        // Swap with heavier particle above (steam is lighter/negative mass)
        swapParticles(x, y, x, y - 1);
        return;
    }

    // Rise diagonally
    bool leftOpen = (x - 1 >= 0 && y - 1 >= 0 && !isOccupied(x - 1, y - 1));
    bool rightOpen = (x + 1 < width && y - 1 >= 0 && !isOccupied(x + 1, y - 1));

    if (leftOpen && rightOpen) {
        float random = static_cast<float>(std::rand()) / RAND_MAX;
        int newX = (random < 0.5f) ? x - 1 : x + 1;
        moveParticle(x, y, newX, y - 1);
        return;
    } else if (leftOpen) {
        moveParticle(x, y, x - 1, y - 1);
        return;
    } else if (rightOpen) {
        moveParticle(x, y, x + 1, y - 1);
        return;
    }

    // Steam spreads horizontally like water (affected by viscosity)
    if (config.steam.horizontalFlowSpeed > 0 && shouldUpdateHorizontalMovement(myType)) {
        for (int i = 1; i <= config.steam.horizontalFlowSpeed; ++i) {
            bool leftFree = (x - i >= 0 && !isOccupied(x - i, y));
            bool rightFree = (x + i < width && !isOccupied(x + i, y));

            if (leftFree && rightFree) {
                float random = static_cast<float>(std::rand()) / RAND_MAX;
                int newX = (random < config.steam.waterDispersionChance) ? x - i : x + i;
                moveParticle(x, y, newX, y);
                return;
            } else if (leftFree) {
                moveParticle(x, y, x - i, y);
                return;
            } else if (rightFree) {
                moveParticle(x, y, x + i, y);
                return;
            }
        }
    }
}

void SandSimulator::updateObsidianParticle(int x, int y) {
    // Obsidian is static and heavy, like rock but without groups
    if (y + 1 >= height) return;

    ParticleType myType = getParticleType(x, y);

    // Fall straight down if empty below
    if (!isOccupied(x, y + 1)) {
        moveParticle(x, y, x, y + 1);
        return;
    } else if (canDisplace(myType, getParticleType(x, y + 1))) {
        // Swap with lighter particle below
        swapParticles(x, y, x, y + 1);
    }
    // Otherwise stay in place (obsidian is very stable)
}

void SandSimulator::updateFireParticle(int x, int y) {
    // Fire burns out over time (random chance)
    float random = static_cast<float>(std::rand()) / RAND_MAX;
    if (random < config.fire.randomTumbleChance) {
        // Burn out - convert to empty or steam
        if (random < config.fire.randomTumbleChance * 0.3f) {
            // 30% of burnouts create smoke/steam
            setParticleType(x, y, ParticleType::STEAM);
            colors[y * width + x] = generateRandomColor(
                config.steam.colorR, config.steam.colorG, config.steam.colorB, config.steam.colorVariation);
        } else {
            // 70% just disappear
            setParticleType(x, y, ParticleType::EMPTY);
        }
        return;
    }

    // Temperature-based interactions handled by heat transfer system
    // Fire rises (negative mass - go UP instead of DOWN)
    if (y - 1 < 0) return;

    ParticleType myType = getParticleType(x, y);

    // Rise straight up
    if (!isOccupied(x, y - 1)) {
        moveParticle(x, y, x, y - 1);
        return;
    } else if (canDisplace(myType, getParticleType(x, y - 1))) {
        swapParticles(x, y, x, y - 1);
        return;
    }

    // Rise diagonally
    bool leftOpen = (x - 1 >= 0 && y - 1 >= 0 && !isOccupied(x - 1, y - 1));
    bool rightOpen = (x + 1 < width && y - 1 >= 0 && !isOccupied(x + 1, y - 1));

    if (leftOpen && rightOpen) {
        float rand = static_cast<float>(std::rand()) / RAND_MAX;
        int newX = (rand < 0.5f) ? x - 1 : x + 1;
        moveParticle(x, y, newX, y - 1);
        return;
    } else if (leftOpen) {
        moveParticle(x, y, x - 1, y - 1);
        return;
    } else if (rightOpen) {
        moveParticle(x, y, x + 1, y - 1);
        return;
    }

    // Fire spreads horizontally
    if (config.fire.horizontalFlowSpeed > 0) {
        for (int i = 1; i <= config.fire.horizontalFlowSpeed; ++i) {
            bool leftFree = (x - i >= 0 && !isOccupied(x - i, y));
            bool rightFree = (x + i < width && !isOccupied(x + i, y));

            if (leftFree && rightFree) {
                float rand = static_cast<float>(std::rand()) / RAND_MAX;
                int newX = (rand < config.fire.waterDispersionChance) ? x - i : x + i;
                moveParticle(x, y, newX, y);
                return;
            } else if (leftFree) {
                moveParticle(x, y, x - i, y);
                return;
            } else if (rightFree) {
                moveParticle(x, y, x + i, y);
                return;
            }
        }
    }
}

void SandSimulator::updateIceParticle(int x, int y) {
    // Ice behaves like a solid - falls like sand
    updateSandParticle(x, y);
}

void SandSimulator::updateGlassParticle(int x, int y) {
    // Glass behaves like a solid - falls like sand
    updateSandParticle(x, y);
}

// Spacing/density expansion
void SandSimulator::updateParticleSpacing(int x, int y) {
    ParticleType type = getParticleType(x, y);
    if (type == ParticleType::EMPTY) return;

    // Get spacing expansion chance and push distance for this particle type
    float expansionChance = 0.0f;
    int pushDistance = 1;
    switch (type) {
        case ParticleType::SAND:
            expansionChance = config.sand.spacingExpansionChance;
            pushDistance = config.sand.spacingPushDistance;
            break;
        case ParticleType::WATER:
            expansionChance = config.water.spacingExpansionChance;
            pushDistance = config.water.spacingPushDistance;
            break;
        case ParticleType::ROCK:
            expansionChance = config.rock.spacingExpansionChance;
            pushDistance = config.rock.spacingPushDistance;
            break;
        case ParticleType::LAVA:
            expansionChance = config.lava.spacingExpansionChance;
            pushDistance = config.lava.spacingPushDistance;
            break;
        case ParticleType::STEAM:
            expansionChance = config.steam.spacingExpansionChance;
            pushDistance = config.steam.spacingPushDistance;
            break;
        case ParticleType::OBSIDIAN:
            expansionChance = config.obsidian.spacingExpansionChance;
            pushDistance = config.obsidian.spacingPushDistance;
            break;
        case ParticleType::FIRE:
            expansionChance = config.fire.spacingExpansionChance;
            pushDistance = config.fire.spacingPushDistance;
            break;
        case ParticleType::ICE:
            expansionChance = config.ice.spacingExpansionChance;
            pushDistance = config.ice.spacingPushDistance;
            break;
        case ParticleType::GLASS:
            expansionChance = config.glass.spacingExpansionChance;
            pushDistance = config.glass.spacingPushDistance;
            break;
        default: return;
    }

    if (expansionChance <= 0.0f) return;

    // Check all 4 cardinal directions
    struct Direction {
        int dx, dy;
    };
    Direction directions[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};  // up, down, left, right

    for (const auto& dir : directions) {
        int neighborX = x + dir.dx;
        int neighborY = y + dir.dy;

        // Check if neighbor is in bounds
        if (!inBounds(neighborX, neighborY)) continue;

        // Roll random chance for this direction
        float random = static_cast<float>(std::rand()) / RAND_MAX;
        if (random >= expansionChance) continue;

        // Check if there's a particle at neighbor position
        ParticleType neighborType = getParticleType(neighborX, neighborY);
        if (neighborType == ParticleType::EMPTY) continue;

        // Try to push the neighbor away (pushDistance cells in the same direction)
        int pushX = neighborX + (dir.dx * pushDistance);
        int pushY = neighborY + (dir.dy * pushDistance);

        if (!inBounds(pushX, pushY)) continue;

        // Try to move neighbor to the push position
        if (!isOccupied(pushX, pushY)) {
            moveParticle(neighborX, neighborY, pushX, pushY);
        } else if (canDisplace(neighborType, getParticleType(pushX, pushY))) {
            // If neighbor can displace what's at push position, swap them
            swapParticles(neighborX, neighborY, pushX, pushY);
        }
    }
}

// Velocity-based physics helpers
float SandSimulator::getMass(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND: return config.sand.mass;
        case ParticleType::WATER: return config.water.mass;
        case ParticleType::ROCK: return config.rock.mass;
        case ParticleType::WOOD: return config.wood.mass;
        case ParticleType::LAVA: return config.lava.mass;
        case ParticleType::STEAM: return config.steam.mass;
        case ParticleType::OBSIDIAN: return config.obsidian.mass;
        case ParticleType::FIRE: return config.fire.mass;
        case ParticleType::ICE: return config.ice.mass;
        case ParticleType::GLASS: return config.glass.mass;
        default: return 0.0f;
    }
}

float SandSimulator::getFriction(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND: return config.sand.friction;
        case ParticleType::WATER: return config.water.friction;
        case ParticleType::ROCK: return config.rock.friction;
        case ParticleType::LAVA: return config.lava.friction;
        case ParticleType::STEAM: return config.steam.friction;
        case ParticleType::OBSIDIAN: return config.obsidian.friction;
        case ParticleType::FIRE: return config.fire.friction;
        case ParticleType::ICE: return config.ice.friction;
        case ParticleType::GLASS: return config.glass.friction;
        default: return 0.0f;
    }
}

bool SandSimulator::canDisplace(ParticleType moving, ParticleType stationary) const {
    if (stationary == ParticleType::EMPTY) return true;
    float movingMass = getMass(moving);
    float stationaryMass = getMass(stationary);
    return movingMass > stationaryMass;
}

// Temperature helper functions
float SandSimulator::getBaseTemperature(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND: return config.sand.baseTemperature;
        case ParticleType::WATER: return config.water.baseTemperature;
        case ParticleType::ROCK: return config.rock.baseTemperature;
        case ParticleType::WOOD: return config.wood.baseTemperature;
        case ParticleType::LAVA: return config.lava.baseTemperature;
        case ParticleType::STEAM: return config.steam.baseTemperature;
        case ParticleType::OBSIDIAN: return config.obsidian.baseTemperature;
        case ParticleType::FIRE: return config.fire.baseTemperature;
        case ParticleType::ICE: return config.ice.baseTemperature;
        case ParticleType::GLASS: return config.glass.baseTemperature;
        default: return 20.0f;
    }
}

float SandSimulator::getMeltingPoint(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND: return config.sand.meltingPoint;
        case ParticleType::WATER: return config.water.meltingPoint;
        case ParticleType::ROCK: return config.rock.meltingPoint;
        case ParticleType::WOOD: return config.wood.meltingPoint;
        case ParticleType::LAVA: return config.lava.meltingPoint;
        case ParticleType::STEAM: return config.steam.meltingPoint;
        case ParticleType::OBSIDIAN: return config.obsidian.meltingPoint;
        case ParticleType::FIRE: return config.fire.meltingPoint;
        case ParticleType::ICE: return config.ice.meltingPoint;
        case ParticleType::GLASS: return config.glass.meltingPoint;
        default: return 0.0f;
    }
}

float SandSimulator::getBoilingPoint(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND: return config.sand.boilingPoint;
        case ParticleType::WATER: return config.water.boilingPoint;
        case ParticleType::ROCK: return config.rock.boilingPoint;
        case ParticleType::WOOD: return config.wood.boilingPoint;
        case ParticleType::LAVA: return config.lava.boilingPoint;
        case ParticleType::STEAM: return config.steam.boilingPoint;
        case ParticleType::OBSIDIAN: return config.obsidian.boilingPoint;
        case ParticleType::FIRE: return config.fire.boilingPoint;
        case ParticleType::ICE: return config.ice.boilingPoint;
        case ParticleType::GLASS: return config.glass.boilingPoint;
        default: return 10000.0f;
    }
}

float SandSimulator::getHeatCapacity(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND: return config.sand.heatCapacity;
        case ParticleType::WATER: return config.water.heatCapacity;
        case ParticleType::ROCK: return config.rock.heatCapacity;
        case ParticleType::WOOD: return config.wood.heatCapacity;
        case ParticleType::LAVA: return config.lava.heatCapacity;
        case ParticleType::STEAM: return config.steam.heatCapacity;
        case ParticleType::OBSIDIAN: return config.obsidian.heatCapacity;
        case ParticleType::FIRE: return config.fire.heatCapacity;
        case ParticleType::ICE: return config.ice.heatCapacity;
        case ParticleType::GLASS: return config.glass.heatCapacity;
        default: return 1.0f;
    }
}

float SandSimulator::getThermalConductivity(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND: return config.sand.thermalConductivity;
        case ParticleType::WATER: return config.water.thermalConductivity;
        case ParticleType::ROCK: return config.rock.thermalConductivity;
        case ParticleType::WOOD: return config.wood.thermalConductivity;
        case ParticleType::LAVA: return config.lava.thermalConductivity;
        case ParticleType::STEAM: return config.steam.thermalConductivity;
        case ParticleType::OBSIDIAN: return config.obsidian.thermalConductivity;
        case ParticleType::FIRE: return config.fire.thermalConductivity;
        case ParticleType::ICE: return config.ice.thermalConductivity;
        case ParticleType::GLASS: return config.glass.thermalConductivity;
        default: return 0.5f;
    }
}

// Wetness/absorption helper functions
float SandSimulator::getMaxSaturation(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND: return config.sand.maxSaturation;
        case ParticleType::WATER: return config.water.maxSaturation;
        case ParticleType::ROCK: return config.rock.maxSaturation;
        case ParticleType::WOOD: return config.wood.maxSaturation;
        case ParticleType::LAVA: return config.lava.maxSaturation;
        case ParticleType::STEAM: return config.steam.maxSaturation;
        case ParticleType::OBSIDIAN: return config.obsidian.maxSaturation;
        case ParticleType::FIRE: return config.fire.maxSaturation;
        case ParticleType::ICE: return config.ice.maxSaturation;
        case ParticleType::GLASS: return config.glass.maxSaturation;
        default: return 0.0f;
    }
}

// Heat transfer and phase change
void SandSimulator::updateHeatTransfer(int x, int y) {
    int idx = y * width + x;
    ParticleType type = getParticleType(x, y);
    if (type == ParticleType::EMPTY) return;

    float myTemp = temperature[idx];
    float myHeatCapacity = getHeatCapacity(type);
    float myConductivity = getThermalConductivity(type);

    // Check all 8 neighbors for heat transfer
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    float totalHeatTransfer = 0.0f;

    for (int dir = 0; dir < 8; ++dir) {
        int nx = x + dx[dir];
        int ny = y + dy[dir];

        if (!inBounds(nx, ny)) continue;

        ParticleType neighborType = getParticleType(nx, ny);
        if (neighborType == ParticleType::EMPTY) continue;

        int nIdx = ny * width + nx;
        float neighborTemp = temperature[nIdx];
        float neighborConductivity = getThermalConductivity(neighborType);

        // Calculate heat transfer based on temperature difference and conductivity
        float avgConductivity = (myConductivity + neighborConductivity) * 0.5f;
        float tempDiff = neighborTemp - myTemp;
        float heatFlow = tempDiff * avgConductivity * config.energyConversionFactor;

        totalHeatTransfer += heatFlow;
    }

    // Update temperature based on heat capacity (higher capacity = slower temp change)
    temperature[idx] += totalHeatTransfer / myHeatCapacity;
}

void SandSimulator::checkPhaseChange(int x, int y) {
    int idx = y * width + x;
    ParticleType type = getParticleType(x, y);
    if (type == ParticleType::EMPTY) return;

    float temp = temperature[idx];
    float meltingPt = getMeltingPoint(type);
    float boilingPt = getBoilingPoint(type);

    // ICE -> WATER (melting)
    if (type == ParticleType::ICE && temp >= meltingPt) {
        setParticleType(x, y, ParticleType::WATER);
        colors[idx] = generateRandomColor(
            config.water.colorR, config.water.colorG, config.water.colorB, config.water.colorVariation);
        temperature[idx] = temp;  // Keep current temperature
        velocities[idx] = {0.0f, 0.0f};  // Reset velocity on phase change
    }
    // WATER -> ICE (freezing)
    else if (type == ParticleType::WATER && temp < meltingPt) {
        setParticleType(x, y, ParticleType::ICE);
        colors[idx] = generateRandomColor(
            config.ice.colorR, config.ice.colorG, config.ice.colorB, config.ice.colorVariation);
        temperature[idx] = temp;  // Keep current temperature
        velocities[idx] = {0.0f, 0.0f};  // Reset velocity on phase change
    }
    // WATER -> STEAM (boiling)
    else if (type == ParticleType::WATER && temp >= boilingPt) {
        setParticleType(x, y, ParticleType::STEAM);
        colors[idx] = generateRandomColor(
            config.steam.colorR, config.steam.colorG, config.steam.colorB, config.steam.colorVariation);
        temperature[idx] = temp;  // Keep current temperature
        velocities[idx] = {0.0f, 0.0f};  // Reset velocity on phase change
    }
    // STEAM -> WATER (condensing) - IMPORTANT: Reset upward velocity!
    else if (type == ParticleType::STEAM && temp < boilingPt) {
        setParticleType(x, y, ParticleType::WATER);
        colors[idx] = generateRandomColor(
            config.water.colorR, config.water.colorG, config.water.colorB, config.water.colorVariation);
        temperature[idx] = temp;  // Keep current temperature
        velocities[idx] = {0.0f, 0.0f};  // Reset velocity - critical for steam->water!
    }
    // SAND -> GLASS (melting)
    else if (type == ParticleType::SAND && temp >= meltingPt) {
        setParticleType(x, y, ParticleType::GLASS);
        colors[idx] = generateRandomColor(
            config.glass.colorR, config.glass.colorG, config.glass.colorB, config.glass.colorVariation);
        temperature[idx] = temp;  // Keep current temperature
        velocities[idx] = {0.0f, 0.0f};  // Reset velocity on phase change
    }
    // GLASS -> SAND (solidifying - note: doesn't typically happen, glass would just cool)
    // Omitting reverse for now since glass doesn't naturally revert to sand

    // LAVA -> OBSIDIAN (solidifying)
    else if (type == ParticleType::LAVA && temp < meltingPt) {
        setParticleType(x, y, ParticleType::OBSIDIAN);
        colors[idx] = generateRandomColor(
            config.obsidian.colorR, config.obsidian.colorG, config.obsidian.colorB, config.obsidian.colorVariation);
        temperature[idx] = temp;  // Keep current temperature
        velocities[idx] = {0.0f, 0.0f};  // Reset velocity on phase change
    }
    // NOTE: Obsidian never melts back to lava - it's permanently solid
    // WOOD -> FIRE (ignition/combustion)
    else if (type == ParticleType::WOOD && temp >= boilingPt) {
        setParticleType(x, y, ParticleType::FIRE);
        colors[idx] = generateRandomColor(
            config.fire.colorR, config.fire.colorG, config.fire.colorB, config.fire.colorVariation);
        temperature[idx] = 800.0f;  // Fire is very hot
        velocities[idx] = {0.0f, 0.0f};  // Reset velocity on phase change
        attachmentGroup[idx] = 0;  // Fire is not part of wood cluster anymore
        particleAge[idx] = 0;  // Reset age for fire lifetime
    }
}

void SandSimulator::checkContactReactions(int x, int y) {
    int idx = y * width + x;
    ParticleType type = getParticleType(x, y);
    if (type == ParticleType::EMPTY) return;

    float myTemp = temperature[idx];

    // Check all 8 neighbors for reactions
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int dir = 0; dir < 8; ++dir) {
        int nx = x + dx[dir];
        int ny = y + dy[dir];

        if (!inBounds(nx, ny)) continue;

        ParticleType neighborType = getParticleType(nx, ny);
        if (neighborType == ParticleType::EMPTY) continue;

        int nIdx = ny * width + nx;
        float neighborTemp = temperature[nIdx];

        // LAVA + SAND -> OBSIDIAN + GLASS
        // When very hot lava contacts sand, lava cools to obsidian, sand melts to glass
        if (type == ParticleType::LAVA && neighborType == ParticleType::SAND) {
            if (myTemp > 800.0f) {  // Lava must be hot enough
                // Cool lava significantly (transfer heat to sand)
                float heatTransfer = 400.0f;  // Significant heat loss
                temperature[idx] -= heatTransfer / getHeatCapacity(type);
                temperature[nIdx] += heatTransfer / getHeatCapacity(neighborType);

                // If lava cooled below melting point, it becomes obsidian (handled by phase change)
                // If sand heated above melting point, it becomes glass (handled by phase change)
            }
        }
        else if (type == ParticleType::SAND && neighborType == ParticleType::LAVA) {
            float lavaTemp = neighborTemp;
            if (lavaTemp > 800.0f) {  // Lava must be hot enough
                // Heat sand significantly
                float heatTransfer = 400.0f;
                temperature[nIdx] -= heatTransfer / getHeatCapacity(neighborType);
                temperature[idx] += heatTransfer / getHeatCapacity(type);
            }
        }

        // FIRE + WATER -> STEAM (fire heats water rapidly)
        else if (type == ParticleType::FIRE && neighborType == ParticleType::WATER) {
            if (myTemp > 500.0f) {
                float heatTransfer = 300.0f;
                temperature[nIdx] += heatTransfer / getHeatCapacity(neighborType);
            }
        }
        else if (type == ParticleType::WATER && neighborType == ParticleType::FIRE) {
            if (neighborTemp > 500.0f) {
                float heatTransfer = 300.0f;
                temperature[idx] += heatTransfer / getHeatCapacity(type);
            }
        }

        // LAVA + WATER -> OBSIDIAN + STEAM (water cools lava rapidly)
        else if (type == ParticleType::LAVA && neighborType == ParticleType::WATER) {
            // Water rapidly cools lava - instant solidification!
            // Convert lava to obsidian immediately
            setParticleType(x, y, ParticleType::OBSIDIAN);
            colors[idx] = generateRandomColor(
                config.obsidian.colorR, config.obsidian.colorG, config.obsidian.colorB, config.obsidian.colorVariation);
            temperature[idx] = 400.0f;  // Cool to safe temperature
            velocities[idx] = {0.0f, 0.0f};

            // Convert water to steam immediately
            setParticleType(nx, ny, ParticleType::STEAM);
            colors[nIdx] = generateRandomColor(
                config.steam.colorR, config.steam.colorG, config.steam.colorB, config.steam.colorVariation);
            temperature[nIdx] = 150.0f;  // Heat water way above boiling point
            velocities[nIdx] = {0.0f, 0.0f};
        }
        else if (type == ParticleType::WATER && neighborType == ParticleType::LAVA) {
            // Water rapidly cools lava - instant solidification!
            // Convert water to steam immediately
            setParticleType(x, y, ParticleType::STEAM);
            colors[idx] = generateRandomColor(
                config.steam.colorR, config.steam.colorG, config.steam.colorB, config.steam.colorVariation);
            temperature[idx] = 150.0f;
            velocities[idx] = {0.0f, 0.0f};

            // Convert lava to obsidian immediately
            setParticleType(nx, ny, ParticleType::OBSIDIAN);
            colors[nIdx] = generateRandomColor(
                config.obsidian.colorR, config.obsidian.colorG, config.obsidian.colorB, config.obsidian.colorVariation);
            temperature[nIdx] = 400.0f;
            velocities[nIdx] = {0.0f, 0.0f};
        }

        // HOT PARTICLES + WOOD -> IGNITE WOOD
        // Fire, lava, or any very hot particle can ignite wood
        else if (type == ParticleType::WOOD && neighborTemp > 300.0f) {
            // Neighbor is hot enough to ignite wood
            float heatTransfer = 200.0f;
            temperature[idx] += heatTransfer / getHeatCapacity(type);
        }
        else if (neighborType == ParticleType::WOOD && myTemp > 300.0f) {
            // This particle is hot enough to ignite neighbor wood
            float heatTransfer = 200.0f;
            temperature[nIdx] += heatTransfer / getHeatCapacity(neighborType);
        }
    }
}

// Wetness absorption and spreading
void SandSimulator::updateWetnessAbsorption(int x, int y) {
    int idx = y * width + x;
    ParticleType type = getParticleType(x, y);
    if (type == ParticleType::EMPTY) return;

    float myMaxSat = getMaxSaturation(type);

    // Only absorbent materials (maxSaturation > 0) can absorb water
    if (myMaxSat <= 0.0f) return;

    float myWetness = wetness[idx];

    // If already fully saturated, can't absorb more
    if (myWetness >= myMaxSat) return;

    // Check all 8 neighbors for water particles
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int dir = 0; dir < 8; ++dir) {
        int nx = x + dx[dir];
        int ny = y + dy[dir];

        if (!inBounds(nx, ny)) continue;

        ParticleType neighborType = getParticleType(nx, ny);

        // Only absorb from water particles
        if (neighborType != ParticleType::WATER) continue;

        int nIdx = ny * width + nx;

        // Calculate how much water can be absorbed
        float absorptionRate = config.wetnessAbsorptionRate;  // Configurable absorption rate
        float spaceLeft = myMaxSat - myWetness;
        float toAbsorb = std::min(absorptionRate, spaceLeft);

        // Transfer wetness
        wetness[idx] += toAbsorb;

        // Water particle loses some "mass" - if it's depleted enough, it disappears
        // We track this by reducing the water's wetness (using it as a "remaining water" counter)
        // Initialize water particles to 1.0 "units" of water
        float waterRemaining = wetness[nIdx];
        if (waterRemaining <= 0.0f) waterRemaining = 1.0f;  // First time, initialize

        waterRemaining -= toAbsorb * 3.0f;  // Water depletes faster than sand absorbs

        if (waterRemaining <= 0.0f) {
            // Water particle fully absorbed - remove it
            setParticleType(nx, ny, ParticleType::EMPTY);
            colors[nIdx] = {0, 0, 0};
            velocities[nIdx] = {0.0f, 0.0f};
            temperature[nIdx] = 20.0f;
            wetness[nIdx] = 0.0f;
        } else {
            wetness[nIdx] = waterRemaining;
        }

        // Wake chunks if any meaningful absorption occurred
        // Use a very low threshold to allow propagation across chunks
        if (toAbsorb > 0.0001f) {
            wakeChunkAtPosition(x, y);
            wakeChunkAtPosition(nx, ny);
        }

        // Only absorb from one water particle per frame
        break;
    }
}

void SandSimulator::updateWetnessSpreading(int x, int y) {
    int idx = y * width + x;
    ParticleType type = getParticleType(x, y);
    if (type == ParticleType::EMPTY) return;

    float myMaxSat = getMaxSaturation(type);

    // Only materials that can hold wetness spread it
    if (myMaxSat <= 0.0f) return;

    float myWetness = wetness[idx];

    // Nothing to spread if dry or below threshold (optimization: don't process micro-wetness)
    if (myWetness <= 0.0f) return;
    if (myWetness < config.wetnessMinimumThreshold) return;

    // Check all 8 neighbors for spreading wetness
    const int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int dir = 0; dir < 8; ++dir) {
        int nx = x + dx[dir];
        int ny = y + dy[dir];

        if (!inBounds(nx, ny)) continue;

        ParticleType neighborType = getParticleType(nx, ny);
        if (neighborType == ParticleType::EMPTY) continue;

        float neighborMaxSat = getMaxSaturation(neighborType);

        // Only spread to materials that can absorb
        if (neighborMaxSat <= 0.0f) continue;

        int nIdx = ny * width + nx;
        float neighborWetness = wetness[nIdx];

        // Calculate wetness difference (as ratio of max saturation)
        float myRatio = myWetness / myMaxSat;
        float neighborRatio = neighborWetness / neighborMaxSat;

        // Only spread if I'm wetter (relatively)
        if (myRatio <= neighborRatio) continue;

        // Spread wetness to equalize (with some entropy/slowness)
        float spreadRate = config.wetnessSpreadRate;  // Configurable spread rate
        float diff = myRatio - neighborRatio;
        float toSpread = diff * spreadRate;

        // Convert ratio back to actual wetness amounts
        float actualSpread = toSpread * myMaxSat;

        // Make sure neighbor doesn't exceed their max
        float neighborSpaceLeft = neighborMaxSat - neighborWetness;
        actualSpread = std::min(actualSpread, neighborSpaceLeft);
        actualSpread = std::min(actualSpread, myWetness);  // Can't give more than I have

        // Transfer wetness
        wetness[idx] -= actualSpread;
        wetness[nIdx] += actualSpread;

        // Wake neighbor chunk if any meaningful wetness crossed chunk boundary
        // Use a very low threshold to allow propagation across chunks
        if (actualSpread > 0.0001f) {
            int myChunkX = x / chunkWidth;
            int myChunkY = y / chunkHeight;
            int neighborChunkX = nx / chunkWidth;
            int neighborChunkY = ny / chunkHeight;

            if (myChunkX != neighborChunkX || myChunkY != neighborChunkY) {
                wakeChunk(neighborChunkX, neighborChunkY);
            }
        }
    }
}

void SandSimulator::updateParticleVelocity(int x, int y) {
    int idx = y * width + x;
    ParticleType type = getParticleType(x, y);
    if (type == ParticleType::EMPTY) return;

    float oldVY = velocities[idx].vy;

    // Apply gravity
    velocities[idx].vy += config.gravity;

    // Apply air resistance
    velocities[idx].vx *= (1.0f - config.airResistance);
    velocities[idx].vy *= (1.0f - config.airResistance);

    // Apply friction if on ground
    if (y + 1 < height && isOccupied(x, y + 1)) {
        float friction = getFriction(type);
        velocities[idx].vx *= (1.0f - friction);
    }

    // Clamp velocity
    float maxVel = 10.0f;
    if (velocities[idx].vx > maxVel) velocities[idx].vx = maxVel;
    if (velocities[idx].vx < -maxVel) velocities[idx].vx = -maxVel;
    if (velocities[idx].vy > maxVel) velocities[idx].vy = maxVel;
    if (velocities[idx].vy < -maxVel) velocities[idx].vy = -maxVel;

    // Debug: log particles near center column
    if (debugFrameCount <= 10 && (x >= width/2 - 2 && x <= width/2 + 2) && y >= height - 20) {
        std::ostringstream oss;
        oss << "  UpdateVel (" << x << "," << y << ") "
            << (type == ParticleType::SAND ? "SAND" : "WATER")
            << " vel: (" << velocities[idx].vx << "," << velocities[idx].vy << ")";
        logDebug(oss.str());
    }
}

bool SandSimulator::tryMoveWithVelocity(int x, int y, float vx, float vy) {
    // Round velocity instead of truncating to properly handle sub-pixel velocities
    int targetX = x + static_cast<int>(std::round(vx));
    int targetY = y + static_cast<int>(std::round(vy));

    // Debug: log particles in top rows at center column
    bool shouldLog = (debugFrameCount <= 10 && y < 3 && x == width / 2);

    if (shouldLog) {
        std::ostringstream oss;
        oss << "  TryMove (" << x << "," << y << ") vel=("
            << vx << "," << vy << ") target=(" << targetX << "," << targetY << ")";
        logDebug(oss.str());
    }

    // If velocity rounds to 0 (no movement), check if we should add diagonal velocity
    if (targetX == x && targetY == y) {
        if (y + 1 < height && isOccupied(x, y + 1)) {
            // Get material-specific diagonal slide properties
            ParticleType type = getParticleType(x, y);
            const ParticleTypeConfig& typeConfig = (type == ParticleType::SAND) ? config.sand : config.water;

            // Resting on something - check for diagonal openings
            bool leftOpen = (x > 0 && y + 1 < height && !isOccupied(x - 1, y + 1));
            bool rightOpen = (x + 1 < width && y + 1 < height && !isOccupied(x + 1, y + 1));

            // Only add horizontal velocity if we don't already have significant horizontal movement
            if (std::abs(velocities[y * width + x].vx) < typeConfig.diagonalSlideThreshold) {
                if (leftOpen && rightOpen) {
                    float random = static_cast<float>(std::rand()) / RAND_MAX;
                    velocities[y * width + x].vx = (random < 0.5f) ? -typeConfig.diagonalSlideVelocity : typeConfig.diagonalSlideVelocity;
                    if (shouldLog) {
                        logDebug("    -> RESTING, added diagonal velocity (both open)");
                    }
                } else if (leftOpen) {
                    velocities[y * width + x].vx = -typeConfig.diagonalSlideVelocity;
                    if (shouldLog) {
                        logDebug("    -> RESTING, added left velocity");
                    }
                } else if (rightOpen) {
                    velocities[y * width + x].vx = typeConfig.diagonalSlideVelocity;
                    if (shouldLog) {
                        logDebug("    -> RESTING, added right velocity");
                    }
                }
            }
        }
        if (shouldLog) {
            logDebug("    -> NO MOVEMENT");
        }
        return false;
    }

    // Try direct movement
    if (inBounds(targetX, targetY)) {
        ParticleType targetType = getParticleType(targetX, targetY);
        if (canDisplace(getParticleType(x, y), targetType)) {
            if (targetType != ParticleType::EMPTY) {
                // Swap if heavier displaces lighter
                ParticleType myType = getParticleType(x, y);
                ParticleColor myColor = colors[y * width + x];
                ParticleVelocity myVel = velocities[y * width + x];

                ParticleColor theirColor = colors[targetY * width + targetX];
                ParticleVelocity theirVel = velocities[targetY * width + targetX];

                setParticleType(x, y, targetType);
                colors[y * width + x] = theirColor;
                velocities[y * width + x] = theirVel;

                setParticleType(targetX, targetY, myType);
                colors[targetY * width + targetX] = myColor;
                velocities[targetY * width + targetX] = myVel;

                if (shouldLog) {
                    logDebug("    -> SWAPPED");
                }
            } else {
                moveParticle(x, y, targetX, targetY);
                if (shouldLog) {
                    logDebug("    -> MOVED");
                }
            }
            return true;
        }
    }

    // Try just Y movement if X is blocked
    if (inBounds(x, targetY) && canDisplace(getParticleType(x, y), getParticleType(x, targetY))) {
        moveParticle(x, y, x, targetY);
        velocities[targetY * width + x].vx *= 0.5f;
        if (shouldLog) {
            logDebug("    -> MOVED Y only");
        }
        return true;
    }

    // Try just X movement if Y is blocked
    if (inBounds(targetX, y) && canDisplace(getParticleType(x, y), getParticleType(targetX, y))) {
        moveParticle(x, y, targetX, y);
        velocities[y * width + targetX].vy = 0;
        if (shouldLog) {
            logDebug("    -> MOVED X only");
        }
        return true;
    }

    // Collision - particle can't move, check if it should slide diagonally
    if (y + 1 < height && isOccupied(x, y + 1)) {
        // Get material-specific diagonal slide properties
        ParticleType type = getParticleType(x, y);
        const ParticleTypeConfig& typeConfig = (type == ParticleType::SAND) ? config.sand : config.water;

        // Something directly below - try to add diagonal velocity
        bool leftOpen = (x > 0 && y + 1 < height && !isOccupied(x - 1, y + 1));
        bool rightOpen = (x + 1 < width && y + 1 < height && !isOccupied(x + 1, y + 1));

        // Only add horizontal velocity if we don't already have significant horizontal movement
        if (std::abs(velocities[y * width + x].vx) < typeConfig.diagonalSlideThreshold) {
            if (leftOpen && rightOpen) {
                // Both diagonals open - add random horizontal velocity
                float random = static_cast<float>(std::rand()) / RAND_MAX;
                velocities[y * width + x].vx = (random < 0.5f) ? -typeConfig.diagonalSlideVelocity : typeConfig.diagonalSlideVelocity;
                velocities[y * width + x].vy *= 0.5f; // Reduce vertical velocity
                if (shouldLog) {
                    logDebug("    -> BLOCKED, added diagonal velocity");
                }
            } else if (leftOpen) {
                velocities[y * width + x].vx = -typeConfig.diagonalSlideVelocity;
                velocities[y * width + x].vy *= 0.5f;
                if (shouldLog) {
                    logDebug("    -> BLOCKED, added left velocity");
                }
            } else if (rightOpen) {
                velocities[y * width + x].vx = typeConfig.diagonalSlideVelocity;
                velocities[y * width + x].vy *= 0.5f;
                if (shouldLog) {
                    logDebug("    -> BLOCKED, added right velocity");
                }
            } else {
                // Completely stuck
                velocities[y * width + x].vx *= 0.3f;
                velocities[y * width + x].vy *= 0.3f;
                if (shouldLog) {
                    logDebug("    -> COMPLETELY BLOCKED");
                }
            }
        } else {
            // Already has horizontal velocity, just dampen it
            velocities[y * width + x].vx *= 0.3f;
            velocities[y * width + x].vy *= 0.3f;
            if (shouldLog) {
                logDebug("    -> BLOCKED (has horizontal velocity)");
            }
        }
    } else {
        // General collision
        velocities[y * width + x].vx *= 0.3f;
        velocities[y * width + x].vy *= 0.3f;
        if (shouldLog) {
            logDebug("    -> BLOCKED (general)");
        }
    }
    return false;
}

void SandSimulator::applyVelocityMovement(int x, int y) {
    int idx = y * width + x;
    ParticleType type = getParticleType(x, y);
    if (type == ParticleType::EMPTY) return;

    ParticleVelocity vel = velocities[idx];

    tryMoveWithVelocity(x, y, vel.vx, vel.vy);
}

void SandSimulator::update() {
    if (debugFrameCount <= 10) {
        std::ostringstream oss;
        oss << "\n=== FRAME " << debugFrameCount << " ===";
        logDebug(oss.str());
    }

    // Build optimization data structures
    buildRowSkipList();
    updateChunkActivity();

    // Run multiple passes based on fall_speed
    for (int step = 0; step < config.fallSpeed; ++step) {
        // Clear rock group processing tracker for this frame
        processedRockGroupsThisFrame.clear();

        spawnParticles();

        // HYBRID PHYSICS + PARALLEL PROCESSING + ROW SKIPPING
        // Step 1: Update settled state for active rows only
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < height; ++y) {
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS

            int chunkY = y / chunkHeight;
            for (int x = 0; x < width; ++x) {
                int chunkX = x / chunkWidth;
                if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS

                updateSettledState(x, y);
            }
        }

        // Step 2: Update velocities for UNSETTLED particles (in active rows only)
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < height; ++y) {
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS

            int chunkY = y / chunkHeight;
            for (int x = 0; x < width; ++x) {
                int chunkX = x / chunkWidth;
                if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS

                int idx = y * width + x;
                if (!isSettled[idx]) {
                    updateParticleVelocity(x, y);
                }
            }
        }

        // Step 3: Apply movement - EVEN rows in parallel, then ODD rows
        // PHASE 1: Even rows (0, 2, 4, ...) - these don't touch each other
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = height - 2; y >= 0; y -= 2) {  // Even rows
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS

            int chunkY = y / chunkHeight;

            if (config.processLeftToRight) {
                for (int x = 0; x < width; ++x) {
                    int chunkX = x / chunkWidth;
                    if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS

                    int idx = y * width + x;
                    ParticleType type = getParticleType(x, y);
                    if (type == ParticleType::EMPTY) continue;

                    if (isSettled[idx]) {
                        // SETTLED: Use cheap cellular automaton physics
                        if (type == ParticleType::SAND) updateSandParticle(x, y);
                        else if (type == ParticleType::WATER) updateWaterParticle(x, y);
                        else if (type == ParticleType::ROCK) updateRockParticle(x, y);
                        else if (type == ParticleType::WOOD) updateWoodParticle(x, y);
                        else if (type == ParticleType::LAVA) updateLavaParticle(x, y);
                        else if (type == ParticleType::STEAM) updateSteamParticle(x, y);
                        else if (type == ParticleType::OBSIDIAN) updateObsidianParticle(x, y);
                        else if (type == ParticleType::FIRE) updateFireParticle(x, y);
                        else if (type == ParticleType::ICE) updateIceParticle(x, y);
                        else if (type == ParticleType::GLASS) updateGlassParticle(x, y);
                    } else {
                        // UNSETTLED: Use smooth velocity physics
                        applyVelocityMovement(x, y);
                    }
                }
            } else {
                for (int x = width - 1; x >= 0; --x) {
                    int chunkX = x / chunkWidth;
                    if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS

                    int idx = y * width + x;
                    ParticleType type = getParticleType(x, y);
                    if (type == ParticleType::EMPTY) continue;

                    if (isSettled[idx]) {
                        // SETTLED: Use cheap cellular automaton physics
                        if (type == ParticleType::SAND) updateSandParticle(x, y);
                        else if (type == ParticleType::WATER) updateWaterParticle(x, y);
                        else if (type == ParticleType::ROCK) updateRockParticle(x, y);
                        else if (type == ParticleType::WOOD) updateWoodParticle(x, y);
                        else if (type == ParticleType::LAVA) updateLavaParticle(x, y);
                        else if (type == ParticleType::STEAM) updateSteamParticle(x, y);
                        else if (type == ParticleType::OBSIDIAN) updateObsidianParticle(x, y);
                        else if (type == ParticleType::FIRE) updateFireParticle(x, y);
                        else if (type == ParticleType::ICE) updateIceParticle(x, y);
                        else if (type == ParticleType::GLASS) updateGlassParticle(x, y);
                    } else {
                        // UNSETTLED: Use smooth velocity physics
                        applyVelocityMovement(x, y);
                    }
                }
            }
        }

        // PHASE 2: Odd rows (1, 3, 5, ...) - these don't touch each other
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = height - 1; y >= 0; y -= 2) {  // Odd rows
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS

            int chunkY = y / chunkHeight;

            if (config.processLeftToRight) {
                for (int x = 0; x < width; ++x) {
                    int chunkX = x / chunkWidth;
                    if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS

                    int idx = y * width + x;
                    ParticleType type = getParticleType(x, y);
                    if (type == ParticleType::EMPTY) continue;

                    if (isSettled[idx]) {
                        // SETTLED: Use cheap cellular automaton physics
                        if (type == ParticleType::SAND) updateSandParticle(x, y);
                        else if (type == ParticleType::WATER) updateWaterParticle(x, y);
                        else if (type == ParticleType::ROCK) updateRockParticle(x, y);
                        else if (type == ParticleType::WOOD) updateWoodParticle(x, y);
                        else if (type == ParticleType::LAVA) updateLavaParticle(x, y);
                        else if (type == ParticleType::STEAM) updateSteamParticle(x, y);
                        else if (type == ParticleType::OBSIDIAN) updateObsidianParticle(x, y);
                        else if (type == ParticleType::FIRE) updateFireParticle(x, y);
                        else if (type == ParticleType::ICE) updateIceParticle(x, y);
                        else if (type == ParticleType::GLASS) updateGlassParticle(x, y);
                    } else {
                        // UNSETTLED: Use smooth velocity physics
                        applyVelocityMovement(x, y);
                    }
                }
            } else {
                for (int x = width - 1; x >= 0; --x) {
                    int chunkX = x / chunkWidth;
                    if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS

                    int idx = y * width + x;
                    ParticleType type = getParticleType(x, y);
                    if (type == ParticleType::EMPTY) continue;

                    if (isSettled[idx]) {
                        // SETTLED: Use cheap cellular automaton physics
                        if (type == ParticleType::SAND) updateSandParticle(x, y);
                        else if (type == ParticleType::WATER) updateWaterParticle(x, y);
                        else if (type == ParticleType::ROCK) updateRockParticle(x, y);
                        else if (type == ParticleType::WOOD) updateWoodParticle(x, y);
                        else if (type == ParticleType::LAVA) updateLavaParticle(x, y);
                        else if (type == ParticleType::STEAM) updateSteamParticle(x, y);
                        else if (type == ParticleType::OBSIDIAN) updateObsidianParticle(x, y);
                        else if (type == ParticleType::FIRE) updateFireParticle(x, y);
                        else if (type == ParticleType::ICE) updateIceParticle(x, y);
                        else if (type == ParticleType::GLASS) updateGlassParticle(x, y);
                    } else {
                        // UNSETTLED: Use smooth velocity physics
                        applyVelocityMovement(x, y);
                    }
                }
            }
        }

        // Other updates (in parallel with row skipping)
        // Apply spacing/density expansion
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < height; ++y) {
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS
            int chunkY = y / chunkHeight;
            if (config.processLeftToRight) {
                for (int x = 0; x < width; ++x) {
                    int chunkX = x / chunkWidth;
                    if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS
                    updateParticleSpacing(x, y);
                }
            } else {
                for (int x = width - 1; x >= 0; --x) {
                    int chunkX = x / chunkWidth;
                    if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS
                    updateParticleSpacing(x, y);
                }
            }
        }

        // Apply heat transfer (temperature propagation) - ONLY for recently active chunks
        // Skip for chunks trying to sleep to let them stabilize
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < height; ++y) {
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS
            int chunkY = y / chunkHeight;
            for (int x = 0; x < width; ++x) {
                int chunkX = x / chunkWidth;
                int chunkIdx = chunkY * chunksX + chunkX;

                // Skip sleeping chunks AND chunks trying to sleep (stable for >10 frames)
                if (isChunkSleeping(chunkX, chunkY)) continue;
                if (chunkActivity[chunkIdx].stableFrameCount > 10) continue;

                updateHeatTransfer(x, y);
            }
        }

        // Check for contact reactions (lava+sand->obsidian+glass, fire+water, lava+water->obsidian)
        // NOTE: Don't skip stable chunks - reactions should happen immediately when particles meet!
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < height; ++y) {
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS
            int chunkY = y / chunkHeight;
            for (int x = 0; x < width; ++x) {
                int chunkX = x / chunkWidth;
                if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS
                // Don't skip stable chunks - allow immediate reactions!
                checkContactReactions(x, y);
            }
        }

        // Update wetness absorption (water -> sand)
        // NOTE: Process ALL chunks including sleeping - wetness must propagate everywhere
        // The threshold-based waking in updateWetnessAbsorption wakes chunks only when needed
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < height; ++y) {
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS
            for (int x = 0; x < width; ++x) {
                updateWetnessAbsorption(x, y);
            }
        }

        // Update wetness spreading (sand -> sand)
        // NOTE: Process ALL chunks including sleeping - wetness must propagate everywhere
        // The threshold-based waking in updateWetnessSpreading wakes chunks only when needed
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < height; ++y) {
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS
            for (int x = 0; x < width; ++x) {
                updateWetnessSpreading(x, y);
            }
        }

        // Check for phase changes (water->steam, ice->water, sand->glass, lava->obsidian, wood->fire)
        // NOTE: Don't skip stable chunks - phase changes should happen immediately!
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < height; ++y) {
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS
            int chunkY = y / chunkHeight;
            for (int x = 0; x < width; ++x) {
                int chunkX = x / chunkWidth;
                if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS
                // Don't skip stable chunks - allow immediate phase changes!
                checkPhaseChange(x, y);
            }
        }

        // Age and dissipate temporary particles (steam, fire)
        #pragma omp parallel for schedule(dynamic, 4)
        for (int y = 0; y < height; ++y) {
            if (!rowHasParticles[y]) continue;  // SKIP EMPTY ROWS
            int chunkY = y / chunkHeight;
            for (int x = 0; x < width; ++x) {
                int chunkX = x / chunkWidth;
                if (isChunkSleeping(chunkX, chunkY)) continue;  // SKIP SLEEPING CHUNKS

                int idx = y * width + x;
                ParticleType type = getParticleType(x, y);

                // Age steam and fire particles
                if (type == ParticleType::STEAM || type == ParticleType::FIRE) {
                    particleAge[idx]++;

                    // Dissipate steam after 1800 frames (~30 seconds at 60fps)
                    // Dissipate fire after 1200 frames (~20 seconds at 60fps)
                    int maxAge = (type == ParticleType::STEAM) ? 1800 : 1200;

                    if (particleAge[idx] > maxAge) {
                        setParticleType(x, y, ParticleType::EMPTY);
                    }
                }
            }
        }
    }

    debugFrameCount++;
}
