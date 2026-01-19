#pragma once
#include "Collectible.h"
#include "Ammunition.h"
#include <cmath>
#include <vector>
#include <memory>
#include <string>

class LittlePurpleJumper;
class Sprite;

// Noita-style wand stats
struct WandStats {
    std::string name = "Wand";

    // Mana system
    int maxMana = 100;
    int currentMana = 100;
    float manaRechargeRate = 20.0f;    // Mana per second
    float manaRechargeDelay = 0.5f;    // Seconds before recharge starts after firing

    // Timing
    float castDelay = 0.1f;            // Seconds between each spell cast
    float rechargeTime = 0.5f;         // Seconds to reload after cycling through all spells

    // Modifiers
    float speedMultiplier = 1.0f;      // Global projectile speed modifier
    float damageMultiplier = 1.0f;     // Global damage modifier
    int capacity = 4;                   // Max number of spells/ammunition slots

    // Spread
    float spreadDegrees = 0.0f;        // Additional spread in degrees
};

class Gun : public Collectible {
public:
    Gun();
    ~Gun() = default;

    bool checkCollection(float playerX, float playerY, float playerW, float playerH, bool eKeyPressed);
    void updateEquipped(float playerCenterX, float playerCenterY, float cursorWorldX, float cursorWorldY);
    void renderEquipped(SDL_Renderer* renderer, float cameraX, float cameraY, float scaleX, float scaleY);

    bool isEquipped() const { return equipped; }
    float getAngle() const { return angle; }
    void getMuzzlePosition(float& outX, float& outY) const;
    bool isFlipped() const { return flipped; }

    int damage;

    // Firing
    bool fire(World& world);  // Returns true if fired successfully
    bool canFire(Uint32 currentTime);

    // Ammunition management
    void addAmmunition(std::shared_ptr<Ammunition> ammo);
    void clearAmmunition();
    int getAmmunitionCount() const { return (int)ammunition.size(); }
    std::shared_ptr<Ammunition> getCurrentAmmunition() const;
    std::shared_ptr<Ammunition> getAmmunition(int index) const {
        if (index >= 0 && index < (int)ammunition.size()) return ammunition[index];
        return nullptr;
    }

    // Set sprite sheet for all ammunition (for bullet sprite rendering)
    void setSpriteSheet(Sprite* sheet) {
        bulletSpriteSheet = sheet;
        for (auto& ammo : ammunition) {
            if (ammo) ammo->setSpriteSheet(sheet);
        }
    }

    // Update and render
    void update(float deltaTime);  // Update mana recharge, etc.
    void updateAmmunition(float deltaTime, World& world, std::vector<LittlePurpleJumper>& enemies);
    void renderAmmunition(SDL_Renderer* renderer, std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, float scaleX, float scaleY);

    // Mana system
    int getMana() const { return stats.currentMana; }
    int getMaxMana() const { return stats.maxMana; }
    float getManaPercent() const { return (float)stats.currentMana / stats.maxMana; }

    // Wand stats access
    WandStats& getStats() { return stats; }
    const WandStats& getStats() const { return stats; }

private:
    bool equipped;
    float pivotX, pivotY;
    float angle;
    bool flipped;

    Uint32 lastFireTime;
    float timeSinceLastFire = 0.0f;  // For mana recharge delay

    int spriteWidth;
    int spriteHeight;

    std::vector<std::shared_ptr<Ammunition>> ammunition;
    int currentAmmunition;
    bool cycleComplete = false;       // True when we've fired all spells and need to recharge
    float rechargeTimer = 0.0f;       // Timer for recharge time

    WandStats stats;
    float manaRechargeAccumulator = 0.0f;  // Accumulate fractional mana
    Sprite* bulletSpriteSheet = nullptr;   // Sprite sheet for bullet rendering
};
