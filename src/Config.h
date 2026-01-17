#pragma once
#include <string>

enum class SpawnPosition {
    CENTER,
    LEFT,
    RIGHT
};

struct ParticleTypeConfig {
    int colorR, colorG, colorB;
    int spawnRate;
    SpawnPosition spawnPosition;
    int spawnPositionRandomness;
    int colorVariation;

    // Physics properties
    float mass;
    float friction;
    float restitution;  // bounciness
    float diagonalSlideVelocity;  // horizontal velocity added when sliding diagonally
    float diagonalSlideThreshold; // minimum existing vx before we skip adding more
    int movementFrequency;        // Update movement every N frames (1=every frame, 2=every other frame, etc.)

    // Legacy/simple physics (kept for compatibility)
    float diagonalFallChance;
    int slopeSlideDistance;
    int horizontalSpreadDistance;
    float randomTumbleChance;
    int horizontalFlowSpeed;
    float waterDispersionChance;

    // Spacing/density expansion
    float spacingExpansionChance;  // 0.0-1.0 probability to push neighbors per side
    int spacingPushDistance;       // How many cells away to push (1-3 typical)

    // Temperature properties
    float baseTemperature;         // Base temperature in Celsius
    float meltingPoint;            // Temperature to transition from solid->liquid
    float boilingPoint;            // Temperature to transition from liquid->gas
    float heatCapacity;            // How much energy needed to change temperature (higher = harder to heat)
    float thermalConductivity;     // How easily heat transfers (0.0-1.0, higher = better conductor)

    // Wetness/absorption properties
    float maxSaturation;           // Maximum wetness this material can hold (0.0 = none, 1.0 = fully saturated)

    // Inner rock generation
    int innerRockSpawnChance;
    int innerRockMinSize;
    int innerRockMaxSize;
    float innerRockMinRadius;
    float innerRockMaxRadius;
    float innerRockDarkness;
};

struct Config {
    int windowWidth;
    int windowHeight;
    int pixelScale;
    int fallSpeed;
    bool processLeftToRight;

    // Physics
    float airResistance;
    float particleFallAcceleration;

    // Temperature physics
    float energyConversionFactor;  // Multiplier for heat transfer rate (0.0-1.0)

    // Wetness physics
    float wetnessAbsorptionRate;   // How fast water is absorbed (0.0-1.0 per frame)
    float wetnessSpreadRate;       // How fast wetness spreads between particles (0.0-1.0 per frame)
    float wetnessMinimumThreshold; // Minimum wetness needed before spreading occurs (optimization)

    ParticleTypeConfig sand;
    ParticleTypeConfig water;
    ParticleTypeConfig rock;
    ParticleTypeConfig lava;
    ParticleTypeConfig steam;
    ParticleTypeConfig obsidian;
    ParticleTypeConfig fire;
    ParticleTypeConfig ice;
    ParticleTypeConfig glass;
    ParticleTypeConfig wood;
    ParticleTypeConfig moss;

    Config();
    bool loadFromFile(const std::string& filename);

private:
    void setDefaults();
    void parseLine(const std::string& line);
};
