#include "Config.h"
#include <fstream>
#include <sstream>
#include <iostream>

Config::Config() {
    setDefaults();
}

void Config::setDefaults() {
    windowWidth = 1000;
    windowHeight = 1000;
    pixelScale = 1;
    fallSpeed = 100;
    processLeftToRight = true;

    // Physics defaults
    airResistance = 0.01f;
    particleFallAcceleration = 0.6f;

    // Temperature physics defaults
    energyConversionFactor = 0.1f;  // 10% heat transfer per frame

    // Wetness physics defaults
    wetnessAbsorptionRate = 0.2f;  // 20% absorption per frame (10x faster than before)
    wetnessSpreadRate = 0.1f;      // 10% spread per frame (10x faster than before)
    wetnessMinimumThreshold = 0.05f;  // Only spread if wetness > 5% (prevents micro-wetness processing)

    // Sand defaults
    sand.colorR = 255;
    sand.colorG = 200;
    sand.colorB = 100;
    sand.spawnRate = 5;
    sand.spawnPosition = SpawnPosition::CENTER;
    sand.spawnPositionRandomness = 5;
    sand.colorVariation = 30;
    sand.mass = 2.0f;
    sand.friction = 0.3f;
    sand.restitution = 0.1f;
    sand.diagonalSlideVelocity = 0.75f;
    sand.diagonalSlideThreshold = 0.3f;
    sand.movementFrequency = 1;          // Update every frame
    sand.diagonalFallChance = 0.5f;
    sand.slopeSlideDistance = 2;
    sand.horizontalSpreadDistance = 1;
    sand.randomTumbleChance = 0.01f;
    sand.horizontalFlowSpeed = 0;
    sand.waterDispersionChance = 0.5f;
    sand.spacingExpansionChance = 0.0f;
    sand.spacingPushDistance = 1;
    sand.baseTemperature = 20.0f;        // Room temperature
    sand.meltingPoint = 1700.0f;         // Melts to glass at ~1700°C
    sand.boilingPoint = 2950.0f;         // Boils at ~2950°C
    sand.heatCapacity = 0.8f;            // Moderate heat capacity
    sand.thermalConductivity = 0.3f;     // Poor conductor
    sand.maxSaturation = 0.3f;           // Can absorb water up to 30% saturation
    sand.innerRockSpawnChance = 0;
    sand.innerRockMinSize = 0;
    sand.innerRockMaxSize = 0;
    sand.innerRockMinRadius = 0;
    sand.innerRockMaxRadius = 0;
    sand.innerRockDarkness = 0.5f;

    // Water defaults
    water.colorR = 50;
    water.colorG = 100;
    water.colorB = 255;
    water.spawnRate = 0;
    water.spawnPosition = SpawnPosition::LEFT;
    water.spawnPositionRandomness = 5;
    water.colorVariation = 20;
    water.mass = 1.0f;
    water.friction = 0.05f;
    water.restitution = 0.05f;
    water.diagonalSlideVelocity = 1.5f;
    water.diagonalSlideThreshold = 0.5f;
    water.movementFrequency = 1;         // Update every frame (flows quickly)
    water.diagonalFallChance = 0.5f;
    water.slopeSlideDistance = 0;
    water.horizontalSpreadDistance = 0;
    water.randomTumbleChance = 0.0f;
    water.horizontalFlowSpeed = 3;
    water.waterDispersionChance = 0.5f;
    water.spacingExpansionChance = 0.0f;
    water.spacingPushDistance = 1;
    water.baseTemperature = 20.0f;       // Room temperature
    water.meltingPoint = 0.0f;           // Freezes to ice at 0°C
    water.boilingPoint = 100.0f;         // Boils to steam at 100°C
    water.heatCapacity = 4.2f;           // High heat capacity (takes lots of energy to heat)
    water.thermalConductivity = 0.6f;    // Good conductor
    water.maxSaturation = 0.0f;          // Water doesn't absorb wetness
    water.innerRockSpawnChance = 0;
    water.innerRockMinSize = 0;
    water.innerRockMaxSize = 0;
    water.innerRockMinRadius = 0;
    water.innerRockMaxRadius = 0;
    water.innerRockDarkness = 0.5f;

    // Rock defaults
    rock.colorR = 128;
    rock.colorG = 128;
    rock.colorB = 128;
    rock.spawnRate = 0;
    rock.spawnPosition = SpawnPosition::CENTER;
    rock.spawnPositionRandomness = 0;
    rock.colorVariation = 20;
    rock.mass = 5.0f;  // Very heavy
    rock.friction = 0.5f;
    rock.restitution = 0.2f;
    rock.diagonalSlideVelocity = 0.0f;
    rock.diagonalSlideThreshold = 0.0f;
    rock.movementFrequency = 1;          // Update every frame
    rock.diagonalFallChance = 0.5f;
    rock.slopeSlideDistance = 0;
    rock.horizontalSpreadDistance = 0;
    rock.randomTumbleChance = 0.0f;
    rock.horizontalFlowSpeed = 0;
    rock.waterDispersionChance = 0.5f;
    rock.spacingExpansionChance = 0.0f;
    rock.spacingPushDistance = 1;
    rock.baseTemperature = 20.0f;        // Room temperature
    rock.meltingPoint = 1200.0f;         // Melts at very high temp
    rock.boilingPoint = 2500.0f;         // Boils at extreme temp
    rock.heatCapacity = 0.9f;            // Moderate heat capacity
    rock.thermalConductivity = 0.5f;     // Decent conductor
    rock.maxSaturation = 0.0f;           // Rock doesn't absorb water
    rock.innerRockSpawnChance = 100;
    rock.innerRockMinSize = 3;
    rock.innerRockMaxSize = 7;
    rock.innerRockMinRadius = 1.0f;
    rock.innerRockMaxRadius = 2.5f;
    rock.innerRockDarkness = 0.8f;

    // Lava defaults
    lava.colorR = 255;
    lava.colorG = 100;
    lava.colorB = 0;
    lava.spawnRate = 0;
    lava.spawnPosition = SpawnPosition::CENTER;
    lava.spawnPositionRandomness = 5;
    lava.colorVariation = 30;
    lava.mass = 2.5f;  // A little denser than sand
    lava.friction = 0.2f;
    lava.restitution = 0.1f;
    lava.diagonalSlideVelocity = 0.5f;
    lava.diagonalSlideThreshold = 0.3f;
    lava.movementFrequency = 4;          // Update every 4 frames (slow, viscous)
    lava.diagonalFallChance = 0.5f;
    lava.slopeSlideDistance = 2;
    lava.horizontalSpreadDistance = 1;
    lava.randomTumbleChance = 0.01f;
    lava.horizontalFlowSpeed = 1;
    lava.waterDispersionChance = 0.5f;
    lava.spacingExpansionChance = 0.0f;
    lava.spacingPushDistance = 1;
    lava.baseTemperature = 1000.0f;      // Very hot (1000°C)
    lava.meltingPoint = 600.0f;          // Solidifies to obsidian below 600°C
    lava.boilingPoint = 3000.0f;         // Boils at extreme temp
    lava.heatCapacity = 20.0f;           // Very high heat capacity (cools slowly)
    lava.thermalConductivity = 0.8f;     // Good heat conductor
    lava.maxSaturation = 0.0f;           // Lava doesn't absorb water
    lava.innerRockSpawnChance = 0;
    lava.innerRockMinSize = 0;
    lava.innerRockMaxSize = 0;
    lava.innerRockMinRadius = 0;
    lava.innerRockMaxRadius = 0;
    lava.innerRockDarkness = 0.5f;

    // Steam defaults
    steam.colorR = 240;
    steam.colorG = 240;
    steam.colorB = 240;
    steam.spawnRate = 0;
    steam.spawnPosition = SpawnPosition::CENTER;
    steam.spawnPositionRandomness = 5;
    steam.colorVariation = 10;
    steam.mass = -1.0f;  // Negative mass makes it rise
    steam.friction = 0.01f;
    steam.restitution = 0.05f;
    steam.diagonalSlideVelocity = 1.0f;
    steam.diagonalSlideThreshold = 0.5f;
    steam.movementFrequency = 1;         // Update every frame (rises quickly)
    steam.diagonalFallChance = 0.5f;
    steam.slopeSlideDistance = 0;
    steam.horizontalSpreadDistance = 0;
    steam.randomTumbleChance = 0.0f;
    steam.horizontalFlowSpeed = 2;
    steam.waterDispersionChance = 0.5f;
    steam.spacingExpansionChance = 0.7f;  // 70% chance to push neighbors
    steam.spacingPushDistance = 3;  // Push 3 cells away for stronger expansion
    steam.baseTemperature = 100.0f;      // Just above boiling
    steam.meltingPoint = 0.0f;           // Condenses to water below 100°C (effectively)
    steam.boilingPoint = 10000.0f;       // Already gaseous, doesn't boil further
    steam.heatCapacity = 2.0f;           // Moderate heat capacity
    steam.thermalConductivity = 0.2f;    // Poor conductor (gas)
    steam.maxSaturation = 0.0f;          // Steam doesn't absorb water
    steam.innerRockSpawnChance = 0;
    steam.innerRockMinSize = 0;
    steam.innerRockMaxSize = 0;
    steam.innerRockMinRadius = 0;
    steam.innerRockMaxRadius = 0;
    steam.innerRockDarkness = 0.5f;

    // Obsidian defaults
    obsidian.colorR = 30;
    obsidian.colorG = 20;
    obsidian.colorB = 40;
    obsidian.spawnRate = 0;
    obsidian.spawnPosition = SpawnPosition::CENTER;
    obsidian.spawnPositionRandomness = 0;
    obsidian.colorVariation = 15;
    obsidian.mass = 6.0f;  // Heavier than rock
    obsidian.friction = 0.6f;
    obsidian.restitution = 0.15f;
    obsidian.diagonalSlideVelocity = 0.0f;
    obsidian.diagonalSlideThreshold = 0.0f;
    obsidian.movementFrequency = 1;      // Update every frame
    obsidian.diagonalFallChance = 0.5f;
    obsidian.slopeSlideDistance = 0;
    obsidian.horizontalSpreadDistance = 0;
    obsidian.randomTumbleChance = 0.0f;
    obsidian.horizontalFlowSpeed = 0;
    obsidian.waterDispersionChance = 0.5f;
    obsidian.spacingExpansionChance = 0.0f;
    obsidian.spacingPushDistance = 1;
    obsidian.baseTemperature = 400.0f;   // Cooled solidified lava (below melting point)
    obsidian.meltingPoint = 600.0f;      // Melts back to lava at 600°C
    obsidian.boilingPoint = 3000.0f;     // Boils at extreme temp
    obsidian.heatCapacity = 0.8f;        // Moderate heat capacity
    obsidian.thermalConductivity = 0.5f; // Moderate conductor
    obsidian.maxSaturation = 0.0f;       // Obsidian doesn't absorb water
    obsidian.innerRockSpawnChance = 2000;
    obsidian.innerRockMinSize = 20;
    obsidian.innerRockMaxSize = 100;
    obsidian.innerRockMinRadius = 10.0f;
    obsidian.innerRockMaxRadius = 30.5f;
    obsidian.innerRockDarkness = 0.9f;

    // Fire defaults
    fire.colorR = 255;
    fire.colorG = 100;
    fire.colorB = 0;
    fire.spawnRate = 0;
    fire.spawnPosition = SpawnPosition::CENTER;
    fire.spawnPositionRandomness = 5;
    fire.colorVariation = 80;  // High variation for flickering effect
    fire.mass = -0.3f;  // Light, rises up
    fire.friction = 0.01f;
    fire.restitution = 0.05f;
    fire.diagonalSlideVelocity = 0.8f;
    fire.diagonalSlideThreshold = 0.4f;
    fire.movementFrequency = 1;          // Update every frame (rises quickly)
    fire.diagonalFallChance = 0.5f;
    fire.slopeSlideDistance = 0;
    fire.horizontalSpreadDistance = 2;  // Fire spreads sideways
    fire.randomTumbleChance = 0.05f;  // 5% chance to burn out per frame
    fire.horizontalFlowSpeed = 2;  // Spreads horizontally
    fire.waterDispersionChance = 0.5f;
    fire.spacingExpansionChance = 0.3f;  // Some expansion like gas
    fire.spacingPushDistance = 2;
    fire.baseTemperature = 800.0f;       // Very hot (800°C flames)
    fire.meltingPoint = 0.0f;            // No solid phase
    fire.boilingPoint = 10000.0f;        // Already gaseous/plasma
    fire.heatCapacity = 1.0f;            // Moderate heat capacity
    fire.thermalConductivity = 0.7f;     // Good heat transfer
    fire.maxSaturation = 0.0f;           // Fire doesn't absorb water
    fire.innerRockSpawnChance = 0;
    fire.innerRockMinSize = 0;
    fire.innerRockMaxSize = 0;
    fire.innerRockMinRadius = 0;
    fire.innerRockMaxRadius = 0;
    fire.innerRockDarkness = 0.5f;

    // Ice defaults
    ice.colorR = 200;
    ice.colorG = 230;
    ice.colorB = 255;
    ice.spawnRate = 0;
    ice.spawnPosition = SpawnPosition::CENTER;
    ice.spawnPositionRandomness = 0;
    ice.colorVariation = 15;
    ice.mass = 0.9f;  // Slightly less dense than water
    ice.friction = 0.1f;
    ice.restitution = 0.2f;
    ice.diagonalSlideVelocity = 0.8f;
    ice.diagonalSlideThreshold = 0.2f;
    ice.movementFrequency = 1;           // Update every frame
    ice.diagonalFallChance = 0.5f;
    ice.slopeSlideDistance = 3;  // Ice slides easily
    ice.horizontalSpreadDistance = 0;
    ice.randomTumbleChance = 0.0f;
    ice.horizontalFlowSpeed = 0;
    ice.waterDispersionChance = 0.5f;
    ice.spacingExpansionChance = 0.0f;
    ice.spacingPushDistance = 1;
    ice.baseTemperature = -10.0f;        // Below freezing
    ice.meltingPoint = 0.0f;             // Melts to water at 0°C
    ice.boilingPoint = 100.0f;           // Would become steam if heated past water phase
    ice.heatCapacity = 2.1f;             // Lower than water
    ice.thermalConductivity = 0.4f;      // Poor conductor
    ice.maxSaturation = 0.0f;            // Ice doesn't absorb water
    ice.innerRockSpawnChance = 0;
    ice.innerRockMinSize = 0;
    ice.innerRockMaxSize = 0;
    ice.innerRockMinRadius = 0;
    ice.innerRockMaxRadius = 0;
    ice.innerRockDarkness = 0.5f;

    // Glass defaults
    glass.colorR = 100;
    glass.colorG = 180;
    glass.colorB = 180;
    glass.spawnRate = 0;
    glass.spawnPosition = SpawnPosition::CENTER;
    glass.spawnPositionRandomness = 0;
    glass.colorVariation = 20;
    glass.mass = 2.5f;  // Similar to sand
    glass.friction = 0.4f;
    glass.restitution = 0.2f;
    glass.diagonalSlideVelocity = 0.5f;
    glass.diagonalSlideThreshold = 0.25f;
    glass.movementFrequency = 1;         // Update every frame
    glass.diagonalFallChance = 0.5f;
    glass.slopeSlideDistance = 1;
    glass.horizontalSpreadDistance = 0;
    glass.randomTumbleChance = 0.0f;
    glass.horizontalFlowSpeed = 0;
    glass.waterDispersionChance = 0.5f;
    glass.spacingExpansionChance = 0.0f;
    glass.spacingPushDistance = 1;
    glass.baseTemperature = 1700.0f;     // Hot from melting (cools to 20°C)
    glass.meltingPoint = 1700.0f;        // Melts back to liquid at 1700°C (would become lava-like)
    glass.boilingPoint = 2950.0f;        // Boils at extreme temp
    glass.heatCapacity = 0.8f;           // Similar to sand
    glass.thermalConductivity = 0.7f;    // Better conductor than sand
    glass.maxSaturation = 0.0f;          // Glass doesn't absorb water
    glass.innerRockSpawnChance = 0;
    glass.innerRockMinSize = 0;
    glass.innerRockMaxSize = 0;
    glass.innerRockMinRadius = 0;
    glass.innerRockMaxRadius = 0;
    glass.innerRockDarkness = 0.5f;

    // Wood defaults
    wood.colorR = 139;
    wood.colorG = 90;
    wood.colorB = 43;
    wood.spawnRate = 0;
    wood.spawnPosition = SpawnPosition::CENTER;
    wood.spawnPositionRandomness = 0;
    wood.colorVariation = 20;
    wood.mass = 0.6f;  // Less dense than water (floats!)
    wood.friction = 0.6f;
    wood.restitution = 0.15f;
    wood.diagonalSlideVelocity = 0.0f;
    wood.diagonalSlideThreshold = 0.0f;
    wood.movementFrequency = 1;          // Update every frame
    wood.diagonalFallChance = 0.5f;
    wood.slopeSlideDistance = 0;
    wood.horizontalSpreadDistance = 0;
    wood.randomTumbleChance = 0.0f;
    wood.horizontalFlowSpeed = 0;
    wood.waterDispersionChance = 0.5f;
    wood.spacingExpansionChance = 0.0f;
    wood.spacingPushDistance = 1;
    wood.baseTemperature = 20.0f;        // Room temperature
    wood.meltingPoint = 300.0f;          // Burns/chars at ~300°C
    wood.boilingPoint = 450.0f;          // Combusts at ~450°C
    wood.heatCapacity = 1.7f;            // Moderate heat capacity
    wood.thermalConductivity = 0.15f;    // Poor conductor (insulator)
    wood.maxSaturation = 0.5f;           // Wood absorbs water well
    wood.innerRockSpawnChance = 0;
    wood.innerRockMinSize = 0;
    wood.innerRockMaxSize = 0;
    wood.innerRockMinRadius = 0;
    wood.innerRockMaxRadius = 0;
    wood.innerRockDarkness = 0.5f;

    // Moss defaults
    moss.colorR = 0;
    moss.colorG = 150;
    moss.colorB = 0;
    moss.spawnRate = 0;
    moss.spawnPosition = SpawnPosition::CENTER;
    moss.spawnPositionRandomness = 0;
    moss.colorVariation = 40;
    moss.mass = 0.2f;
    moss.friction = 0.8f;
    moss.restitution = 0.05f;
    moss.diagonalSlideVelocity = 0.0f;
    moss.diagonalSlideThreshold = 0.0f;
    moss.movementFrequency = 100;
    moss.diagonalFallChance = 0.1f;
    moss.slopeSlideDistance = 0;
    moss.horizontalSpreadDistance = 0;
    moss.randomTumbleChance = 0.0f;
    moss.horizontalFlowSpeed = 0;
    moss.waterDispersionChance = 0.1f;
    moss.spacingExpansionChance = 0.0f;
    moss.spacingPushDistance = 1;
    moss.baseTemperature = 20.0f;
    moss.meltingPoint = 200.0f;
    moss.boilingPoint = 300.0f;
    moss.heatCapacity = 1.5f;
    moss.thermalConductivity = 0.2f;
    moss.maxSaturation = 0.8f;
    moss.innerRockSpawnChance = 0;
    moss.innerRockMinSize = 0;
    moss.innerRockMaxSize = 0;
    moss.innerRockMinRadius = 0;
    moss.innerRockMaxRadius = 0;
    moss.innerRockDarkness = 0.5f;
}

bool Config::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open " << filename << ", using defaults\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        parseLine(line);
    }

    file.close();
    return true;
}

void Config::parseLine(const std::string& line) {
    if (line.empty() || line[0] == '#') return;

    std::istringstream iss(line);
    std::string key, equals;

    if (!(iss >> key >> equals)) return;
    if (equals != "=") return;

    // Global settings
    if (key == "window_width") iss >> windowWidth;
    else if (key == "window_height") iss >> windowHeight;
    else if (key == "pixel_scale") iss >> pixelScale;
    else if (key == "fall_speed") iss >> fallSpeed;
    else if (key == "process_left_to_right") {
        std::string value;
        iss >> value;
        processLeftToRight = (value == "true");
    }
    else if (key == "air_resistance") iss >> airResistance;
    else if (key == "particle_fall_acceleration") iss >> particleFallAcceleration;
    else if (key == "energy_conversion_factor") iss >> energyConversionFactor;
    else if (key == "wetness_absorption_rate") iss >> wetnessAbsorptionRate;
    else if (key == "wetness_spread_rate") iss >> wetnessSpreadRate;
    else if (key == "wetness_minimum_threshold") iss >> wetnessMinimumThreshold;
    // Sand settings
    else if (key == "sand_color_r") iss >> sand.colorR;
    else if (key == "sand_color_g") iss >> sand.colorG;
    else if (key == "sand_color_b") iss >> sand.colorB;
    else if (key == "sand_spawn_rate") iss >> sand.spawnRate;
    else if (key == "sand_spawn_position") {
        std::string value;
        iss >> value;
        if (value == "center") sand.spawnPosition = SpawnPosition::CENTER;
        else if (value == "left") sand.spawnPosition = SpawnPosition::LEFT;
        else if (value == "right") sand.spawnPosition = SpawnPosition::RIGHT;
    }
    else if (key == "sand_spawn_position_randomness") iss >> sand.spawnPositionRandomness;
    else if (key == "sand_color_variation") iss >> sand.colorVariation;
    else if (key == "sand_mass") iss >> sand.mass;
    else if (key == "sand_friction") iss >> sand.friction;
    else if (key == "sand_restitution") iss >> sand.restitution;
    else if (key == "sand_diagonal_slide_velocity") iss >> sand.diagonalSlideVelocity;
    else if (key == "sand_diagonal_slide_threshold") iss >> sand.diagonalSlideThreshold;
    else if (key == "sand_diagonal_fall_chance") iss >> sand.diagonalFallChance;
    else if (key == "sand_slope_slide_distance") iss >> sand.slopeSlideDistance;
    else if (key == "sand_horizontal_spread_distance") iss >> sand.horizontalSpreadDistance;
    else if (key == "sand_random_tumble_chance") iss >> sand.randomTumbleChance;
    else if (key == "sand_spacing_expansion_chance") iss >> sand.spacingExpansionChance;
    else if (key == "sand_spacing_push_distance") iss >> sand.spacingPushDistance;
    else if (key == "sand_base_temperature") iss >> sand.baseTemperature;
    else if (key == "sand_melting_point") iss >> sand.meltingPoint;
    else if (key == "sand_boiling_point") iss >> sand.boilingPoint;
    else if (key == "sand_heat_capacity") iss >> sand.heatCapacity;
    else if (key == "sand_thermal_conductivity") iss >> sand.thermalConductivity;
    // Water settings
    else if (key == "water_color_r") iss >> water.colorR;
    else if (key == "water_color_g") iss >> water.colorG;
    else if (key == "water_color_b") iss >> water.colorB;
    else if (key == "water_spawn_rate") iss >> water.spawnRate;
    else if (key == "water_spawn_position") {
        std::string value;
        iss >> value;
        if (value == "center") water.spawnPosition = SpawnPosition::CENTER;
        else if (value == "left") water.spawnPosition = SpawnPosition::LEFT;
        else if (value == "right") water.spawnPosition = SpawnPosition::RIGHT;
    }
    else if (key == "water_spawn_position_randomness") iss >> water.spawnPositionRandomness;
    else if (key == "water_color_variation") iss >> water.colorVariation;
    else if (key == "water_mass") iss >> water.mass;
    else if (key == "water_friction") iss >> water.friction;
    else if (key == "water_restitution") iss >> water.restitution;
    else if (key == "water_diagonal_slide_velocity") iss >> water.diagonalSlideVelocity;
    else if (key == "water_diagonal_slide_threshold") iss >> water.diagonalSlideThreshold;
    else if (key == "water_horizontal_flow_speed") iss >> water.horizontalFlowSpeed;
    else if (key == "water_dispersion_chance") iss >> water.waterDispersionChance;
    else if (key == "water_spacing_expansion_chance") iss >> water.spacingExpansionChance;
    else if (key == "water_spacing_push_distance") iss >> water.spacingPushDistance;
    else if (key == "water_base_temperature") iss >> water.baseTemperature;
    else if (key == "water_melting_point") iss >> water.meltingPoint;
    else if (key == "water_boiling_point") iss >> water.boilingPoint;
    else if (key == "water_heat_capacity") iss >> water.heatCapacity;
    else if (key == "water_thermal_conductivity") iss >> water.thermalConductivity;
    // Rock settings
    else if (key == "rock_color_r") iss >> rock.colorR;
    else if (key == "rock_color_g") iss >> rock.colorG;
    else if (key == "rock_color_b") iss >> rock.colorB;
    else if (key == "rock_color_variation") iss >> rock.colorVariation;
    else if (key == "rock_mass") iss >> rock.mass;
    else if (key == "rock_spacing_expansion_chance") iss >> rock.spacingExpansionChance;
    else if (key == "rock_spacing_push_distance") iss >> rock.spacingPushDistance;
    else if (key == "rock_base_temperature") iss >> rock.baseTemperature;
    else if (key == "rock_melting_point") iss >> rock.meltingPoint;
    else if (key == "rock_boiling_point") iss >> rock.boilingPoint;
    else if (key == "rock_heat_capacity") iss >> rock.heatCapacity;
    else if (key == "rock_thermal_conductivity") iss >> rock.thermalConductivity;
    else if (key == "rock_inner_rock_spawn_chance") iss >> rock.innerRockSpawnChance;
    else if (key == "rock_inner_rock_min_size") iss >> rock.innerRockMinSize;
    else if (key == "rock_inner_rock_max_size") iss >> rock.innerRockMaxSize;
    else if (key == "rock_inner_rock_min_radius") iss >> rock.innerRockMinRadius;
    else if (key == "rock_inner_rock_max_radius") iss >> rock.innerRockMaxRadius;
    else if (key == "rock_inner_rock_darkness") iss >> rock.innerRockDarkness;
    // Lava settings
    else if (key == "lava_color_r") iss >> lava.colorR;
    else if (key == "lava_color_g") iss >> lava.colorG;
    else if (key == "lava_color_b") iss >> lava.colorB;
    else if (key == "lava_spawn_rate") iss >> lava.spawnRate;
    else if (key == "lava_spawn_position") {
        std::string value;
        iss >> value;
        if (value == "center") lava.spawnPosition = SpawnPosition::CENTER;
        else if (value == "left") lava.spawnPosition = SpawnPosition::LEFT;
        else if (value == "right") lava.spawnPosition = SpawnPosition::RIGHT;
    }
    else if (key == "lava_spawn_position_randomness") iss >> lava.spawnPositionRandomness;
    else if (key == "lava_color_variation") iss >> lava.colorVariation;
    else if (key == "lava_mass") iss >> lava.mass;
    else if (key == "lava_friction") iss >> lava.friction;
    else if (key == "lava_restitution") iss >> lava.restitution;
    else if (key == "lava_diagonal_slide_velocity") iss >> lava.diagonalSlideVelocity;
    else if (key == "lava_diagonal_slide_threshold") iss >> lava.diagonalSlideThreshold;
    else if (key == "lava_diagonal_fall_chance") iss >> lava.diagonalFallChance;
    else if (key == "lava_slope_slide_distance") iss >> lava.slopeSlideDistance;
    else if (key == "lava_horizontal_spread_distance") iss >> lava.horizontalSpreadDistance;
    else if (key == "lava_random_tumble_chance") iss >> lava.randomTumbleChance;
    else if (key == "lava_horizontal_flow_speed") iss >> lava.horizontalFlowSpeed;
    else if (key == "lava_water_dispersion_chance") iss >> lava.waterDispersionChance;
    else if (key == "lava_spacing_expansion_chance") iss >> lava.spacingExpansionChance;
    else if (key == "lava_spacing_push_distance") iss >> lava.spacingPushDistance;
    else if (key == "lava_base_temperature") iss >> lava.baseTemperature;
    else if (key == "lava_melting_point") iss >> lava.meltingPoint;
    else if (key == "lava_boiling_point") iss >> lava.boilingPoint;
    else if (key == "lava_heat_capacity") iss >> lava.heatCapacity;
    else if (key == "lava_thermal_conductivity") iss >> lava.thermalConductivity;
    // Steam settings
    else if (key == "steam_color_r") iss >> steam.colorR;
    else if (key == "steam_color_g") iss >> steam.colorG;
    else if (key == "steam_color_b") iss >> steam.colorB;
    else if (key == "steam_spawn_rate") iss >> steam.spawnRate;
    else if (key == "steam_spawn_position") {
        std::string value;
        iss >> value;
        if (value == "center") steam.spawnPosition = SpawnPosition::CENTER;
        else if (value == "left") steam.spawnPosition = SpawnPosition::LEFT;
        else if (value == "right") steam.spawnPosition = SpawnPosition::RIGHT;
    }
    else if (key == "steam_spawn_position_randomness") iss >> steam.spawnPositionRandomness;
    else if (key == "steam_color_variation") iss >> steam.colorVariation;
    else if (key == "steam_mass") iss >> steam.mass;
    else if (key == "steam_friction") iss >> steam.friction;
    else if (key == "steam_restitution") iss >> steam.restitution;
    else if (key == "steam_diagonal_slide_velocity") iss >> steam.diagonalSlideVelocity;
    else if (key == "steam_diagonal_slide_threshold") iss >> steam.diagonalSlideThreshold;
    else if (key == "steam_diagonal_fall_chance") iss >> steam.diagonalFallChance;
    else if (key == "steam_slope_slide_distance") iss >> steam.slopeSlideDistance;
    else if (key == "steam_horizontal_spread_distance") iss >> steam.horizontalSpreadDistance;
    else if (key == "steam_random_tumble_chance") iss >> steam.randomTumbleChance;
    else if (key == "steam_horizontal_flow_speed") iss >> steam.horizontalFlowSpeed;
    else if (key == "steam_water_dispersion_chance") iss >> steam.waterDispersionChance;
    else if (key == "steam_spacing_expansion_chance") iss >> steam.spacingExpansionChance;
    else if (key == "steam_spacing_push_distance") iss >> steam.spacingPushDistance;
    else if (key == "steam_base_temperature") iss >> steam.baseTemperature;
    else if (key == "steam_melting_point") iss >> steam.meltingPoint;
    else if (key == "steam_boiling_point") iss >> steam.boilingPoint;
    else if (key == "steam_heat_capacity") iss >> steam.heatCapacity;
    else if (key == "steam_thermal_conductivity") iss >> steam.thermalConductivity;
    // Obsidian settings
    else if (key == "obsidian_color_r") iss >> obsidian.colorR;
    else if (key == "obsidian_color_g") iss >> obsidian.colorG;
    else if (key == "obsidian_color_b") iss >> obsidian.colorB;
    else if (key == "obsidian_color_variation") iss >> obsidian.colorVariation;
    else if (key == "obsidian_mass") iss >> obsidian.mass;
    else if (key == "obsidian_spacing_expansion_chance") iss >> obsidian.spacingExpansionChance;
    else if (key == "obsidian_spacing_push_distance") iss >> obsidian.spacingPushDistance;
    else if (key == "obsidian_base_temperature") iss >> obsidian.baseTemperature;
    else if (key == "obsidian_melting_point") iss >> obsidian.meltingPoint;
    else if (key == "obsidian_boiling_point") iss >> obsidian.boilingPoint;
    else if (key == "obsidian_heat_capacity") iss >> obsidian.heatCapacity;
    else if (key == "obsidian_thermal_conductivity") iss >> obsidian.thermalConductivity;
    else if (key == "obsidian_inner_rock_spawn_chance") iss >> obsidian.innerRockSpawnChance;
    else if (key == "obsidian_inner_rock_min_size") iss >> obsidian.innerRockMinSize;
    else if (key == "obsidian_inner_rock_max_size") iss >> obsidian.innerRockMaxSize;
    else if (key == "obsidian_inner_rock_min_radius") iss >> obsidian.innerRockMinRadius;
    else if (key == "obsidian_inner_rock_max_radius") iss >> obsidian.innerRockMaxRadius;
    else if (key == "obsidian_inner_rock_darkness") iss >> obsidian.innerRockDarkness;
    // Fire settings
    else if (key == "fire_color_r") iss >> fire.colorR;
    else if (key == "fire_color_g") iss >> fire.colorG;
    else if (key == "fire_color_b") iss >> fire.colorB;
    else if (key == "fire_spawn_rate") iss >> fire.spawnRate;
    else if (key == "fire_color_variation") iss >> fire.colorVariation;
    else if (key == "fire_mass") iss >> fire.mass;
    else if (key == "fire_horizontal_spread_distance") iss >> fire.horizontalSpreadDistance;
    else if (key == "fire_random_tumble_chance") iss >> fire.randomTumbleChance;
    else if (key == "fire_horizontal_flow_speed") iss >> fire.horizontalFlowSpeed;
    else if (key == "fire_spacing_expansion_chance") iss >> fire.spacingExpansionChance;
    else if (key == "fire_spacing_push_distance") iss >> fire.spacingPushDistance;
    else if (key == "fire_base_temperature") iss >> fire.baseTemperature;
    else if (key == "fire_melting_point") iss >> fire.meltingPoint;
    else if (key == "fire_boiling_point") iss >> fire.boilingPoint;
    else if (key == "fire_heat_capacity") iss >> fire.heatCapacity;
    else if (key == "fire_thermal_conductivity") iss >> fire.thermalConductivity;
    // Ice settings
    else if (key == "ice_color_r") iss >> ice.colorR;
    else if (key == "ice_color_g") iss >> ice.colorG;
    else if (key == "ice_color_b") iss >> ice.colorB;
    else if (key == "ice_color_variation") iss >> ice.colorVariation;
    else if (key == "ice_mass") iss >> ice.mass;
    else if (key == "ice_base_temperature") iss >> ice.baseTemperature;
    else if (key == "ice_melting_point") iss >> ice.meltingPoint;
    else if (key == "ice_boiling_point") iss >> ice.boilingPoint;
    else if (key == "ice_heat_capacity") iss >> ice.heatCapacity;
    else if (key == "ice_thermal_conductivity") iss >> ice.thermalConductivity;
    // Glass settings
    else if (key == "glass_color_r") iss >> glass.colorR;
    else if (key == "glass_color_g") iss >> glass.colorG;
    else if (key == "glass_color_b") iss >> glass.colorB;
    else if (key == "glass_color_variation") iss >> glass.colorVariation;
    else if (key == "glass_mass") iss >> glass.mass;
    else if (key == "glass_base_temperature") iss >> glass.baseTemperature;
    else if (key == "glass_melting_point") iss >> glass.meltingPoint;
    else if (key == "glass_boiling_point") iss >> glass.boilingPoint;
    else if (key == "glass_heat_capacity") iss >> glass.heatCapacity;
    else if (key == "glass_thermal_conductivity") iss >> glass.thermalConductivity;
    // Wood settings
    else if (key == "wood_color_r") iss >> wood.colorR;
    else if (key == "wood_color_g") iss >> wood.colorG;
    else if (key == "wood_color_b") iss >> wood.colorB;
    else if (key == "wood_color_variation") iss >> wood.colorVariation;
    else if (key == "wood_mass") iss >> wood.mass;
    else if (key == "wood_base_temperature") iss >> wood.baseTemperature;
    else if (key == "wood_melting_point") iss >> wood.meltingPoint;
    else if (key == "wood_boiling_point") iss >> wood.boilingPoint;
    else if (key == "wood_heat_capacity") iss >> wood.heatCapacity;
    else if (key == "wood_thermal_conductivity") iss >> wood.thermalConductivity;
    else if (key == "wood_max_saturation") iss >> wood.maxSaturation;
    // Moss settings
    else if (key == "moss_color_r") iss >> moss.colorR;
    else if (key == "moss_color_g") iss >> moss.colorG;
    else if (key == "moss_color_b") iss >> moss.colorB;
    else if (key == "moss_color_variation") iss >> moss.colorVariation;
    else if (key == "moss_mass") iss >> moss.mass;
    else if (key == "moss_base_temperature") iss >> moss.baseTemperature;
    else if (key == "moss_melting_point") iss >> moss.meltingPoint;
    else if (key == "moss_boiling_point") iss >> moss.boilingPoint;
    else if (key == "moss_heat_capacity") iss >> moss.heatCapacity;
    else if (key == "moss_thermal_conductivity") iss >> moss.thermalConductivity;
    else if (key == "moss_max_saturation") iss >> moss.maxSaturation;
}
