#pragma once

#include <string>
#include <memory>

class Bullet;
class World;

// Base class for spell modifiers that can be attached to ammunition
class SpellModifier {
public:
    virtual ~SpellModifier() = default;

    // Called when the spell is fired - can modify initial bullet properties
    virtual void onFire(Bullet& bullet) {}

    // Called each frame during bullet update
    virtual void onUpdate(Bullet& bullet, World& world, float deltaTime) {}

    // Called when bullet hits something (before deactivation)
    virtual void onHit(Bullet& bullet, World& world, float hitX, float hitY) {}

    // Modifier metadata
    std::string name = "Unknown";
    std::string description = "";
    int manaCostModifier = 0;  // Added to spell's base mana cost
};

// ============== DAMAGE MODIFIERS ==============

class DamageUpModifier : public SpellModifier {
public:
    DamageUpModifier(float multiplier = 1.5f) : damageMultiplier(multiplier) {
        name = "Damage Up";
        description = "Increases damage";
        manaCostModifier = 5;
    }

    void onFire(Bullet& bullet) override;

private:
    float damageMultiplier;
};

class CriticalHitModifier : public SpellModifier {
public:
    CriticalHitModifier(float chance = 0.2f, float multiplier = 3.0f)
        : critChance(chance), critMultiplier(multiplier) {
        name = "Critical";
        description = "Chance for critical hit";
        manaCostModifier = 8;
    }

    void onFire(Bullet& bullet) override;

private:
    float critChance;
    float critMultiplier;
};

// ============== SPEED MODIFIERS ==============

class SpeedUpModifier : public SpellModifier {
public:
    SpeedUpModifier(float multiplier = 1.5f) : speedMultiplier(multiplier) {
        name = "Speed Up";
        description = "Faster projectile";
        manaCostModifier = 3;
    }

    void onFire(Bullet& bullet) override;

private:
    float speedMultiplier;
};

class SlowModifier : public SpellModifier {
public:
    SlowModifier(float multiplier = 0.5f) : speedMultiplier(multiplier) {
        name = "Slow";
        description = "Slower but more controlled";
        manaCostModifier = -2;  // Actually reduces cost
    }

    void onFire(Bullet& bullet) override;

private:
    float speedMultiplier;
};

// ============== BEHAVIOR MODIFIERS ==============

class BouncingModifier : public SpellModifier {
public:
    BouncingModifier(int maxBounces = 3) : bounces(maxBounces) {
        name = "Bouncing";
        description = "Bounces off surfaces";
        manaCostModifier = 10;
    }

    void onFire(Bullet& bullet) override;

private:
    int bounces;
};

class PiercingModifier : public SpellModifier {
public:
    PiercingModifier(int pierceCount = 2) : pierces(pierceCount) {
        name = "Piercing";
        description = "Passes through enemies";
        manaCostModifier = 15;
    }

    void onFire(Bullet& bullet) override;

private:
    int pierces;
};

class HomingModifier : public SpellModifier {
public:
    HomingModifier(float strength = 200.0f, float range = 100.0f)
        : homingStrength(strength), homingRange(range) {
        name = "Homing";
        description = "Seeks nearby enemies";
        manaCostModifier = 20;
    }

    void onFire(Bullet& bullet) override;

private:
    float homingStrength;
    float homingRange;
};

// ============== EFFECT MODIFIERS ==============

class ExplosionModifier : public SpellModifier {
public:
    ExplosionModifier(float radius = 10.0f, int explosionDamage = 5)
        : explosionRadius(radius), damage(explosionDamage) {
        name = "Explosion";
        description = "Explodes on impact";
        manaCostModifier = 25;
    }

    void onHit(Bullet& bullet, World& world, float hitX, float hitY) override;

private:
    float explosionRadius;
    int damage;
};

class TrailModifier : public SpellModifier {
public:
    enum TrailType { FIRE, POISON, OIL };

    TrailModifier(TrailType type = FIRE) : trailType(type) {
        name = "Trail";
        description = "Leaves trail behind";
        manaCostModifier = 12;
    }

    void onUpdate(Bullet& bullet, World& world, float deltaTime) override;

private:
    TrailType trailType;
    float spawnTimer = 0.0f;
};
