#pragma once

#include "Ammunition.h"
#include "Bullet.h"

// Basic fast projectile - low damage, low mana cost, high speed
class SparkBolt : public Ammunition {
public:
    SparkBolt();
    ~SparkBolt() override = default;

    void fire(World& world, float x, float y, float angle, int damage) override;
    void update(float deltaTime, World& world, std::vector<LittlePurpleJumper>& enemies) override;
    void render(SDL_Renderer* renderer, std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, float scaleX, float scaleY) override;

    void cleanup() override;
    int getActiveBulletCount() const override;

private:
    std::vector<Bullet> bullets;
};
