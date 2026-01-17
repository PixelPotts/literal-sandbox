#ifndef BULLET_H
#define BULLET_H

#include "World.h" // Assuming World is needed for collision detection
#include <cmath>   // For std::sqrt

struct Bullet {
    float x, y;           // World position
    float vx, vy;         // Velocity (normalized direction * speed)
    bool active;          // Is this bullet still alive?
    static constexpr float SPEED = 500.0f;       // Pixels per second
    static constexpr int EXPLOSION_RADIUS = 18;  // Explosion radius on impact
    static constexpr float EXPLOSION_FORCE = 14.0f; // Force applied to particles

    // Trail data
    static constexpr int TRAIL_LENGTH = 10;
    std::vector<std::pair<float, float>> trail;

    Bullet() : x(0), y(0), vx(0), vy(0), active(false) {}

    Bullet(float startX, float startY, float targetX, float targetY)
        : x(startX), y(startY), active(true) {
        float dx = targetX - startX;
        float dy = targetY - startY;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.1f) {
            vx = (dx / len) * SPEED;
            vy = (dy / len) * SPEED;
        } else {
            vx = SPEED; // Default if target is too close
            vy = 0;
        }
    }

    // Update bullet position, return true if hit something
    bool update(World& world, float deltaTime);

    // Draw bullet
    void draw(std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, SDL_Renderer* renderer);
};

#endif // BULLET_H
