#pragma once

// Forward declarations
class World;
class WorldChunk;
enum class ParticleType : unsigned char;

struct TextureParams {
    float spawnChance;
    int minPatchSize;
    int maxPatchSize;
    float minPatchRadius;
    float maxPatchRadius;
    float colorMultiplier;
};

class Texturize {
public:
    void apply(World* world, WorldChunk* chunk, ParticleType targetType, const TextureParams& params);
    void applyBrickTexture(World* world, WorldChunk* chunk);
};
