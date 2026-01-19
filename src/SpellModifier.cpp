#include "SpellModifier.h"
#include "Bullet.h"
#include "World.h"
#include <cstdlib>
#include <cmath>

// ============== DAMAGE MODIFIERS ==============

void DamageUpModifier::onFire(Bullet& bullet) {
    bullet.damage = (int)(bullet.damage * damageMultiplier);
}

void CriticalHitModifier::onFire(Bullet& bullet) {
    float roll = (float)rand() / RAND_MAX;
    if (roll < critChance) {
        bullet.damage = (int)(bullet.damage * critMultiplier);
        bullet.isCritical = true;
    }
}

// ============== SPEED MODIFIERS ==============

void SpeedUpModifier::onFire(Bullet& bullet) {
    bullet.vx *= speedMultiplier;
    bullet.vy *= speedMultiplier;
}

void SlowModifier::onFire(Bullet& bullet) {
    bullet.vx *= speedMultiplier;
    bullet.vy *= speedMultiplier;
}

// ============== BEHAVIOR MODIFIERS ==============

void BouncingModifier::onFire(Bullet& bullet) {
    bullet.bouncesRemaining = bounces;
}

void PiercingModifier::onFire(Bullet& bullet) {
    bullet.piercesRemaining = pierces;
}

void HomingModifier::onFire(Bullet& bullet) {
    bullet.homingStrength = homingStrength;
    bullet.homingRange = homingRange;
}

// ============== EFFECT MODIFIERS ==============

void ExplosionModifier::onHit(Bullet& bullet, World& world, float hitX, float hitY) {
    // Create explosion - destroy particles in radius
    int centerX = (int)hitX;
    int centerY = (int)hitY;
    int radius = (int)explosionRadius;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                int wx = centerX + dx;
                int wy = centerY + dy;
                // Check if the particle is destructible (not rock/obsidian)
                ParticleType pt = world.getParticle(wx, wy);
                if (pt != ParticleType::EMPTY && pt != ParticleType::ROCK && pt != ParticleType::OBSIDIAN) {
                    world.spawnParticleAt(wx, wy, ParticleType::EMPTY);
                }
            }
        }
    }

    // TODO: Deal damage to enemies in radius
}

void TrailModifier::onUpdate(Bullet& bullet, World& world, float deltaTime) {
    spawnTimer += deltaTime;
    if (spawnTimer >= 0.02f) {  // Every 20ms
        spawnTimer = 0.0f;

        int wx = (int)bullet.x;
        int wy = (int)bullet.y;

        // Don't spawn if there's already something there
        if (world.getParticle(wx, wy) == ParticleType::EMPTY) {
            switch (trailType) {
                case FIRE:
                    world.spawnParticleAt(wx, wy, ParticleType::FIRE);
                    break;
                case POISON:
                    // Would need POISON type, use STEAM for now
                    world.spawnParticleAt(wx, wy, ParticleType::STEAM);
                    break;
                case OIL:
                    // Would need OIL type, use WATER for now
                    world.spawnParticleAt(wx, wy, ParticleType::WATER);
                    break;
            }
        }
    }
}
