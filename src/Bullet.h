#ifndef BULLET_H
#define BULLET_H

#include "World.h" // Assuming World is needed for collision detection
#include <cmath>   // For std::sqrt

class LittlePurpleJumper;

// Represents a bullet
struct Bullet {
    float x, y;
    float vx, vy;
    bool active = true;
    int damage;

    // Trail
    std::vector<std::pair<float, float>> trail;
    static const int TRAIL_LENGTH = 10;
    static constexpr float SPEED = 500.0f;

    Bullet(float startX, float startY, float dirX, float dirY, int d)
        : x(startX), y(startY), damage(d) {
        float length = std::sqrt(dirX * dirX + dirY * dirY);
        if (length > 0) {
            vx = (dirX / length) * SPEED;
            vy = (dirY / length) * SPEED;
        } else {
            vx = 0;
            vy = 0;
        }
    }

    // Update bullet position, return true if hit something
    bool update(World& world, float deltaTime, std::vector<LittlePurpleJumper>& enemies);

    // Draw bullet
    void draw(std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, SDL_Renderer* renderer);
};

#endif // BULLET_H
