#include "SandSimulator.h"  // For ParticleType, ParticleColor, ParticleVelocity
#include "WorldChunk.h"

WorldChunk::WorldChunk(int chunkX, int chunkY)
    : chunkX(chunkX)
    , chunkY(chunkY)
    , particleCount(0)
    , sleeping(false)
    , active(false)
    , stableFrameCount(0)
{
    const int SIZE = CHUNK_SIZE * CHUNK_SIZE;

    particles.resize(SIZE, ParticleType::EMPTY);
    colors.resize(SIZE, {0, 0, 0});
    velocities.resize(SIZE, {0.0f, 0.0f});
    temperatures.resize(SIZE, 20.0f);  // Room temperature
    wetness.resize(SIZE, 0.0f);
    settledFlags.resize(SIZE, true);
    freefallFlags.resize(SIZE, false);
    explodingFlags.resize(SIZE, false);
    movedFlags.resize(SIZE, false);
    attachmentGroups.resize(SIZE, 0);
    ages.resize(SIZE, 0);
}

ParticleType WorldChunk::getParticle(int localX, int localY) const {
    if (!inBounds(localX, localY)) return ParticleType::EMPTY;
    return particles[getIndex(localX, localY)];
}

void WorldChunk::setParticle(int localX, int localY, ParticleType type) {
    if (!inBounds(localX, localY)) return;
    int idx = getIndex(localX, localY);

    // Update particle count
    if (particles[idx] == ParticleType::EMPTY && type != ParticleType::EMPTY) {
        particleCount++;
    } else if (particles[idx] != ParticleType::EMPTY && type == ParticleType::EMPTY) {
        particleCount--;
    }

    particles[idx] = type;
}

ParticleColor WorldChunk::getColor(int localX, int localY) const {
    if (!inBounds(localX, localY)) return {0, 0, 0};
    return colors[getIndex(localX, localY)];
}

void WorldChunk::setColor(int localX, int localY, ParticleColor color) {
    if (!inBounds(localX, localY)) return;
    colors[getIndex(localX, localY)] = color;
}

ParticleVelocity WorldChunk::getVelocity(int localX, int localY) const {
    if (!inBounds(localX, localY)) return {0.0f, 0.0f};
    return velocities[getIndex(localX, localY)];
}

void WorldChunk::setVelocity(int localX, int localY, ParticleVelocity vel) {
    if (!inBounds(localX, localY)) return;
    velocities[getIndex(localX, localY)] = vel;
}

float WorldChunk::getTemperature(int localX, int localY) const {
    if (!inBounds(localX, localY)) return 20.0f;
    return temperatures[getIndex(localX, localY)];
}

void WorldChunk::setTemperature(int localX, int localY, float temp) {
    if (!inBounds(localX, localY)) return;
    temperatures[getIndex(localX, localY)] = temp;
}

float WorldChunk::getWetness(int localX, int localY) const {
    if (!inBounds(localX, localY)) return 0.0f;
    return wetness[getIndex(localX, localY)];
}

void WorldChunk::setWetness(int localX, int localY, float wet) {
    if (!inBounds(localX, localY)) return;
    wetness[getIndex(localX, localY)] = wet;
}

bool WorldChunk::isSettled(int localX, int localY) const {
    if (!inBounds(localX, localY)) return true;
    return settledFlags[getIndex(localX, localY)];
}

void WorldChunk::setSettled(int localX, int localY, bool settled) {
    if (!inBounds(localX, localY)) return;
    settledFlags[getIndex(localX, localY)] = settled;
}

bool WorldChunk::isFreefalling(int localX, int localY) const {
    if (!inBounds(localX, localY)) return false;
    return freefallFlags[getIndex(localX, localY)];
}

void WorldChunk::setFreefalling(int localX, int localY, bool freefall) {
    if (!inBounds(localX, localY)) return;
    freefallFlags[getIndex(localX, localY)] = freefall;
}

bool WorldChunk::isExploding(int localX, int localY) const {
    if (!inBounds(localX, localY)) return false;
    return explodingFlags[getIndex(localX, localY)];
}

void WorldChunk::setExploding(int localX, int localY, bool exploding) {
    if (!inBounds(localX, localY)) return;
    explodingFlags[getIndex(localX, localY)] = exploding;
}

bool WorldChunk::hasMovedThisFrame(int localX, int localY) const {
    if (!inBounds(localX, localY)) return false;
    return movedFlags[getIndex(localX, localY)];
}

void WorldChunk::setMovedThisFrame(int localX, int localY, bool moved) {
    if (!inBounds(localX, localY)) return;
    movedFlags[getIndex(localX, localY)] = moved;
}

int WorldChunk::getAttachmentGroup(int localX, int localY) const {
    if (!inBounds(localX, localY)) return 0;
    return attachmentGroups[getIndex(localX, localY)];
}

void WorldChunk::setAttachmentGroup(int localX, int localY, int group) {
    if (!inBounds(localX, localY)) return;
    attachmentGroups[getIndex(localX, localY)] = group;
}

int WorldChunk::getParticleAge(int localX, int localY) const {
    if (!inBounds(localX, localY)) return 0;
    return ages[getIndex(localX, localY)];
}

void WorldChunk::setParticleAge(int localX, int localY, int age) {
    if (!inBounds(localX, localY)) return;
    ages[getIndex(localX, localY)] = age;
}

void WorldChunk::clearMovedFlags() {
    std::fill(movedFlags.begin(), movedFlags.end(), false);
}
