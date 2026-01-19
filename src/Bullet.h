#ifndef BULLET_H
#define BULLET_H

#include "World.h"
#include "MainSprite.h"
#include <cmath>

class LittlePurpleJumper;
class Sprite;
struct BulletTypeConfig;

struct Bullet {
    float x, y;
    float vx, vy;
    float angle;               // Current angle (radians) for sprite rotation
    bool active = true;
    int damage;

    // Trail
    std::vector<std::pair<float, float>> trail;
    static const int TRAIL_LENGTH = 10;
    static constexpr float SPEED = 500.0f;

    // Modifier-affected properties
    int bouncesRemaining = 0;
    int piercesRemaining = 0;
    float homingStrength = 0.0f;
    float homingRange = 0.0f;
    bool isCritical = false;
    float lifetime = 5.0f;

    // Visual properties
    Uint32 color = 0xFF69B4;
    SpriteRegion spriteRegion = {0, 0, 0, 0};  // {0,0,0,0} = no sprite, use color
    bool animated = false;
    int frameCount = 1;
    float frameTime = 0.1f;
    float animTimer = 0.0f;
    int currentFrame = 0;

    Bullet(float startX, float startY, float dirX, float dirY, int d)
        : x(startX), y(startY), damage(d) {
        float length = std::sqrt(dirX * dirX + dirY * dirY);
        if (length > 0) {
            vx = (dirX / length) * SPEED;
            vy = (dirY / length) * SPEED;
            angle = std::atan2(dirY, dirX);
        } else {
            vx = 0;
            vy = 0;
            angle = 0;
        }
    }

    // Apply configuration from BulletConfig
    void applyConfig(const BulletTypeConfig& config);

    bool update(World& world, float deltaTime, std::vector<LittlePurpleJumper>& enemies);

    // Draw bullet - uses sprite if available, otherwise falls back to color
    void draw(std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight,
              float cameraX, float cameraY, SDL_Renderer* renderer, Sprite* spriteSheet = nullptr);
};

#endif // BULLET_H
