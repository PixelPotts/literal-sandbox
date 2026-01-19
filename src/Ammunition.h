#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include <memory>
#include <string>
#include "World.h"
#include "MainSprite.h"  // For SpriteRegion

class LittlePurpleJumper;
class SpellModifier;
class Sprite;

class Ammunition {
public:
    virtual ~Ammunition() = default;

    virtual void fire(World& world, float x, float y, float angle, int damage) = 0;
    virtual void update(float deltaTime, World& world, std::vector<LittlePurpleJumper>& enemies) = 0;
    virtual void render(SDL_Renderer* renderer, std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, float scaleX, float scaleY) = 0;

    virtual void cleanup() {}
    virtual int getActiveBulletCount() const { return 0; }

    void addModifier(std::shared_ptr<SpellModifier> modifier) {
        modifiers.push_back(modifier);
    }

    void clearModifiers() {
        modifiers.clear();
    }

    // Set sprite sheet for bullet rendering
    void setSpriteSheet(Sprite* sheet) { spriteSheet = sheet; }
    Sprite* getSpriteSheet() const { return spriteSheet; }

    // Ammunition properties
    std::string name = "Unknown";
    std::string description = "";
    int manaCost = 10;
    float spread = 0.0f;
    float lifetime = 5.0f;
    int projectileCount = 1;
    Uint32 projectileColor = 0xFF69B4;

    // Sprite config
    SpriteRegion spriteRegion = {0, 0, 0, 0};
    bool animated = false;
    int frameCount = 1;
    float frameTime = 0.1f;

    // Trail config
    bool leavesTrail = false;
    int trailParticleType = 0;
    float trailInterval = 0.03f;

protected:
    std::vector<std::shared_ptr<SpellModifier>> modifiers;
    Sprite* spriteSheet = nullptr;
};
