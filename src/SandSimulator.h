#pragma once
#include "Config.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

enum class ParticleType : unsigned char {
    EMPTY = 0,
    SAND = 1,
    WATER = 2,
    ROCK = 3,
    LAVA = 4,
    STEAM = 5,
    OBSIDIAN = 6,
    FIRE = 7,
    ICE = 8,
    GLASS = 9,
    WOOD = 10,
    MOSS = 11
};

struct ParticleColor {
    unsigned char r, g, b;
};

struct ParticleVelocity {
    float vx, vy;
};

class SandSimulator {
public:
    SandSimulator(const Config& config);
    ~SandSimulator();

    void update();
    void spawnParticles();
    void spawnParticleAt(int x, int y, ParticleType type);
    void spawnRockCluster(int centerX, int centerY);
    void spawnWoodCluster(int centerX, int centerY);
    ParticleType getParticleType(int x, int y) const;
    void setParticleType(int x, int y, ParticleType type);
    bool isOccupied(int x, int y) const;
    ParticleColor getColor(int x, int y) const;

    int getWidth() const { return width; }
    int getHeight() const { return height; }

    // Get particle count for a specific type (cached for performance)
    int getParticleCount(ParticleType type) const;

    // Debug visualization
    int getChunkWidth() const { return chunkWidth; }
    int getChunkHeight() const { return chunkHeight; }
    int getChunksX() const { return chunksX; }
    int getChunksY() const { return chunksY; }
    bool isChunkActiveForDebug(int chunkX, int chunkY) const;
    bool isChunkSleepingForDebug(int chunkX, int chunkY) const;

private:
    int width;
    int height;
    std::vector<ParticleType> grid;
    std::vector<ParticleColor> colors;
    std::vector<ParticleVelocity> velocities;
    std::vector<float> temperature;  // Temperature in degrees Celsius
    std::vector<float> wetness;      // Wetness level (0.0-1.0, relative to maxSaturation)
    std::vector<bool> isSettled;     // false = velocity physics (smooth), true = cellular physics (cheap)
    std::vector<int> attachmentGroup; // 0 = not attached, >0 = group ID
    std::vector<int> particleAge;    // Frames since spawn (for steam/fire dissipation)
    int nextAttachmentGroupId;
    std::unordered_map<int, std::vector<std::pair<int, int>>> rockGroupCache; // Cache of groupId -> particle positions
    std::unordered_set<int> processedRockGroupsThisFrame; // Track which groups already moved this frame
    std::mutex rockGroupMutex; // Protects processedRockGroupsThisFrame for thread safety
    Config config;

    // Performance optimization: Activity tracking
    struct ChunkInfo {
        bool isActive;
        int lastActiveFrame;
        bool isSleeping;        // True if chunk has been stable for a while
        int stableFrameCount;   // How many frames this chunk has been stable
    };
    std::vector<ChunkInfo> chunkActivity;
    std::vector<bool> rowHasParticles;  // Quick row skip list
    int chunkWidth;
    int chunkHeight;
    int chunksX;
    int chunksY;

    // Sleep thresholds
    static const int FRAMES_UNTIL_SLEEP = 30;  // Sleep after 30 stable frames (~0.5 seconds at 60fps)

    // Sequential spawning
    int spawnCounter;
    ParticleType currentSpawnType;

    // Cached particle counts (for performance)
    mutable int particleCounts[10];  // Index matches ParticleType enum values
    void incrementParticleCount(ParticleType type);
    void decrementParticleCount(ParticleType type);

    // Debug
    int debugFrameCount;
    void logDebug(const std::string& message);

    bool inBounds(int x, int y) const;

    // Generic helpers
    ParticleColor generateRandomColor(int baseR, int baseG, int baseB, int variation);
    int clamp(int value, int min, int max);
    bool shouldUpdateMovement(ParticleType type) const;
    bool shouldUpdateHorizontalMovement(ParticleType type) const;

    // Hybrid physics helpers
    bool shouldBeSettled(int x, int y) const;
    void updateSettledState(int x, int y);

    // Activity tracking helpers
    void buildRowSkipList();
    void updateChunkActivity();
    bool isChunkActive(int chunkX, int chunkY) const;
    void activateChunk(int chunkX, int chunkY);
    void activateChunkAtPosition(int x, int y);
    bool chunkHasParticles(int chunkX, int chunkY) const;
    bool anyNeighborChunkActive(int chunkX, int chunkY) const;
    int getChunkIndex(int chunkX, int chunkY) const;

    // Sleep tracking helpers
    bool isChunkSleeping(int chunkX, int chunkY) const;
    void wakeChunk(int chunkX, int chunkY);
    void wakeChunkAtPosition(int x, int y);
    void wakeNeighborChunks(int chunkX, int chunkY);
    bool isChunkStable(int chunkX, int chunkY) const;

    // Legacy simple physics
    void updateSandParticle(int x, int y);
    bool trySandSlopeSlide(int x, int y);
    bool trySandHorizontalSpread(int x, int y);
    bool trySandRandomTumble(int x, int y);
    void updateWaterParticle(int x, int y);
    void updateRockParticle(int x, int y);
    void updateWoodParticle(int x, int y);
    void updateLavaParticle(int x, int y);
    void updateSteamParticle(int x, int y);
    void updateObsidianParticle(int x, int y);
    void updateFireParticle(int x, int y);
    void updateIceParticle(int x, int y);
    void updateGlassParticle(int x, int y);

    // Spacing/density expansion
    void updateParticleSpacing(int x, int y);

    // Temperature and heat transfer
    void updateHeatTransfer(int x, int y);
    void checkPhaseChange(int x, int y);
    void checkContactReactions(int x, int y);
    float getBaseTemperature(ParticleType type) const;
    float getMeltingPoint(ParticleType type) const;
    float getBoilingPoint(ParticleType type) const;
    float getHeatCapacity(ParticleType type) const;
    float getThermalConductivity(ParticleType type) const;

    // Wetness and absorption
    void updateWetnessAbsorption(int x, int y);
    void updateWetnessSpreading(int x, int y);
    float getMaxSaturation(ParticleType type) const;

    // Velocity-based physics
    void updateParticleVelocity(int x, int y);
    void applyVelocityMovement(int x, int y);
    float getMass(ParticleType type) const;
    float getFriction(ParticleType type) const;
    bool canDisplace(ParticleType moving, ParticleType stationary) const;

    // Movement
    void moveParticle(int fromX, int fromY, int toX, int toY);
    void swapParticles(int x1, int y1, int x2, int y2);
    bool tryMoveWithVelocity(int x, int y, float vx, float vy);
};
