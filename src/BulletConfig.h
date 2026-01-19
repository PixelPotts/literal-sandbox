#pragma once

#include <cstdint>
#include "MainSprite.h"  // For SpriteRegion

// Configuration for each bullet/ammunition type
struct BulletTypeConfig {
    // Identity
    const char* name;
    const char* description;

    // Costs
    int manaCost;

    // Projectile behavior
    float speed;            // Multiplier on base speed (1.0 = normal)
    float spread;           // Radians of random spread
    float lifetime;         // Seconds before despawn
    int damage;             // Bonus damage added to base
    int projectileCount;    // Bullets per shot

    // Built-in modifiers
    int bounces;            // 0 = no bouncing
    int pierces;            // 0 = no piercing
    float homingStrength;   // 0 = no homing
    float homingRange;      // Detection radius for homing

    // Visuals
    uint32_t color;         // Fallback color if no sprite
    SpriteRegion sprite;    // {x, y, w, h} on sprite sheet, {0,0,0,0} = use color
    bool animated;          // Whether sprite animates
    int frameCount;         // Number of animation frames (horizontal)
    float frameTime;        // Seconds per frame

    // Trail
    bool leavesTrail;       // Whether to leave particle trail
    int trailParticleType;  // ParticleType enum value (7 = FIRE)
    float trailInterval;    // Seconds between trail spawns
};

// ============== BULLET TYPE CONFIGURATIONS ==============

namespace BulletConfigs {

    // SparkBolt - Fast, weak, cheap
    constexpr BulletTypeConfig SparkBolt = {
        .name = "Spark Bolt",
        .description = "A fast, weak projectile",
        .manaCost = 3,
        .speed = 1.5f,
        .spread = 0.05f,
        .lifetime = 2.0f,
        .damage = 0,
        .projectileCount = 1,
        .bounces = 0,
        .pierces = 0,
        .homingStrength = 0.0f,
        .homingRange = 0.0f,
        .color = 0xFFFF00,      // Yellow
        .sprite = {24, 0, 8, 8}, // Sprite at (24,0) size 8x8
        .animated = false,
        .frameCount = 1,
        .frameTime = 0.1f,
        .leavesTrail = false,
        .trailParticleType = 0,
        .trailInterval = 0.0f
    };

    // FireBolt - Medium damage, leaves fire trail
    constexpr BulletTypeConfig FireBolt = {
        .name = "Fire Bolt",
        .description = "A fiery projectile that ignites",
        .manaCost = 15,
        .speed = 1.0f,
        .spread = 0.08f,
        .lifetime = 2.5f,
        .damage = 5,
        .projectileCount = 1,
        .bounces = 0,
        .pierces = 0,
        .homingStrength = 0.0f,
        .homingRange = 0.0f,
        .color = 0xFF4500,      // Orange-red
        .sprite = {32, 0, 8, 8}, // Sprite at (32,0) size 8x8
        .animated = false,
        .frameCount = 1,
        .frameTime = 0.1f,
        .leavesTrail = true,
        .trailParticleType = 7, // FIRE
        .trailInterval = 0.03f
    };

    // MagicMissile - Homing projectile
    constexpr BulletTypeConfig MagicMissile = {
        .name = "Magic Missile",
        .description = "A homing projectile that seeks enemies",
        .manaCost = 25,
        .speed = 0.8f,
        .spread = 0.0f,
        .lifetime = 4.0f,
        .damage = 0,
        .projectileCount = 1,
        .bounces = 0,
        .pierces = 0,
        .homingStrength = 300.0f,
        .homingRange = 150.0f,
        .color = 0xFF00FF,      // Magenta
        .sprite = {40, 0, 8, 8}, // Sprite at (40,0) size 8x8
        .animated = false,
        .frameCount = 1,
        .frameTime = 0.1f,
        .leavesTrail = false,
        .trailParticleType = 0,
        .trailInterval = 0.0f
    };

    // BouncingBolt - Bounces off surfaces
    constexpr BulletTypeConfig BouncingBolt = {
        .name = "Bouncing Bolt",
        .description = "A projectile that bounces off surfaces",
        .manaCost = 8,
        .speed = 1.0f,
        .spread = 0.0f,
        .lifetime = 5.0f,
        .damage = 0,
        .projectileCount = 1,
        .bounces = 3,
        .pierces = 0,
        .homingStrength = 0.0f,
        .homingRange = 0.0f,
        .color = 0x00FFFF,      // Cyan
        .sprite = {48, 0, 8, 8}, // Sprite at (48,0) size 8x8
        .animated = false,
        .frameCount = 1,
        .frameTime = 0.1f,
        .leavesTrail = false,
        .trailParticleType = 0,
        .trailInterval = 0.0f
    };

    // Energy Sphere - Slow but powerful
    constexpr BulletTypeConfig EnergySphere = {
        .name = "Energy Sphere",
        .description = "A slow but powerful energy ball",
        .manaCost = 40,
        .speed = 0.5f,
        .spread = 0.0f,
        .lifetime = 6.0f,
        .damage = 15,
        .projectileCount = 1,
        .bounces = 0,
        .pierces = 1,
        .homingStrength = 0.0f,
        .homingRange = 0.0f,
        .color = 0x9933FF,      // Purple
        .sprite = {56, 0, 8, 8}, // Sprite at (56,0) size 8x8
        .animated = true,
        .frameCount = 4,
        .frameTime = 0.1f,
        .leavesTrail = false,
        .trailParticleType = 0,
        .trailInterval = 0.0f
    };

    // Scatter Shot - Multiple weak projectiles
    constexpr BulletTypeConfig ScatterShot = {
        .name = "Scatter Shot",
        .description = "Fires multiple projectiles in a spread",
        .manaCost = 12,
        .speed = 1.2f,
        .spread = 0.3f,
        .lifetime = 1.5f,
        .damage = -2,
        .projectileCount = 5,
        .bounces = 0,
        .pierces = 0,
        .homingStrength = 0.0f,
        .homingRange = 0.0f,
        .color = 0xFFFFFF,      // White
        .sprite = {64, 0, 4, 4}, // Smaller sprite at (64,0) size 4x4
        .animated = false,
        .frameCount = 1,
        .frameTime = 0.1f,
        .leavesTrail = false,
        .trailParticleType = 0,
        .trailInterval = 0.0f
    };

} // namespace BulletConfigs
