#pragma once
#include "Config.h"
#include <vector>
#include <cstdint>
#include <memory>

// Forward declarations - actual definitions are in SandSimulator.h
enum class ParticleType : unsigned char;
struct ParticleColor;
struct ParticleVelocity;

// A 512x512 chunk of the world
class WorldChunk {
public:
    static constexpr int CHUNK_SIZE = 512;

    WorldChunk(int chunkX, int chunkY);
    ~WorldChunk() = default;

    // Accessors
    int getChunkX() const { return chunkX; }
    int getChunkY() const { return chunkY; }

    // World coordinates of top-left corner
    int getWorldX() const { return chunkX * CHUNK_SIZE; }
    int getWorldY() const { return chunkY * CHUNK_SIZE; }

    // Particle access (local coordinates 0-511)
    ParticleType getParticle(int localX, int localY) const;
    void setParticle(int localX, int localY, ParticleType type);

    ParticleColor getColor(int localX, int localY) const;
    void setColor(int localX, int localY, ParticleColor color);

    ParticleVelocity getVelocity(int localX, int localY) const;
    void setVelocity(int localX, int localY, ParticleVelocity vel);

    float getTemperature(int localX, int localY) const;
    void setTemperature(int localX, int localY, float temp);

    float getWetness(int localX, int localY) const;
    void setWetness(int localX, int localY, float wet);

    bool isSettled(int localX, int localY) const;
    void setSettled(int localX, int localY, bool settled);

    bool isFreefalling(int localX, int localY) const;
    void setFreefalling(int localX, int localY, bool freefall);

    bool isExploding(int localX, int localY) const;
    void setExploding(int localX, int localY, bool exploding);

    bool hasMovedThisFrame(int localX, int localY) const;
    void setMovedThisFrame(int localX, int localY, bool moved);

    int getAttachmentGroup(int localX, int localY) const;
    void setAttachmentGroup(int localX, int localY, int group);

    int getParticleAge(int localX, int localY) const;
    void setParticleAge(int localX, int localY, int age);

    // Bulk operations
    void clearMovedFlags();
    bool isEmpty() const { return particleCount == 0; }
    int getParticleCount() const { return particleCount; }

    // Sleep state
    bool isSleeping() const { return sleeping; }
    void setSleeping(bool sleep) { sleeping = sleep; }
    bool isActive() const { return active; }
    void setActive(bool act) { active = act; }
    int getStableFrameCount() const { return stableFrameCount; }
    void incrementStableFrames() { stableFrameCount++; }
    void resetStableFrames() { stableFrameCount = 0; }

    // Check if coordinates are valid
    static bool inBounds(int localX, int localY) {
        return localX >= 0 && localX < CHUNK_SIZE && localY >= 0 && localY < CHUNK_SIZE;
    }

    // Direct array access for fast simulation
    std::vector<ParticleType>& getParticleGrid() { return particles; }
    std::vector<ParticleColor>& getColorGrid() { return colors; }
    std::vector<ParticleVelocity>& getVelocityGrid() { return velocities; }
    std::vector<float>& getTemperatureGrid() { return temperatures; }
    std::vector<float>& getWetnessGrid() { return wetness; }
    std::vector<bool>& getSettledGrid() { return settledFlags; }
    std::vector<bool>& getFreefallGrid() { return freefallFlags; }
    std::vector<bool>& getExplodingGrid() { return explodingFlags; }
    std::vector<bool>& getMovedGrid() { return movedFlags; }
    std::vector<int>& getAttachmentGrid() { return attachmentGroups; }
    std::vector<int>& getAgeGrid() { return ages; }

    const std::vector<ParticleType>& getParticleGrid() const { return particles; }
    const std::vector<ParticleColor>& getColorGrid() const { return colors; }

private:
    int chunkX, chunkY;  // Chunk position in chunk coordinates
    int particleCount;
    bool sleeping;
    bool active;
    int stableFrameCount;

    // Particle data arrays (CHUNK_SIZE * CHUNK_SIZE elements each)
    std::vector<ParticleType> particles;
    std::vector<ParticleColor> colors;
    std::vector<ParticleVelocity> velocities;
    std::vector<float> temperatures;
    std::vector<float> wetness;
    std::vector<bool> settledFlags;
    std::vector<bool> freefallFlags;
    std::vector<bool> explodingFlags;
    std::vector<bool> movedFlags;
    std::vector<int> attachmentGroups;
    std::vector<int> ages;

    int getIndex(int localX, int localY) const {
        return localY * CHUNK_SIZE + localX;
    }
};

