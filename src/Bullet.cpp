#include "Bullet.h"
#include "SDL.h" // For Uint32
#include <iostream>

#include "Bullet.h"
#include "SDL.h" // For Uint32
#include <iostream>
#include "LittlePurpleJumper.h"

// Update bullet position, return true if hit something
bool Bullet::update(World& world, float deltaTime, std::vector<LittlePurpleJumper>& enemies) {
    if (!active) return false;

    // Store current position for trail
    trail.insert(trail.begin(), {x, y});
    if (trail.size() > TRAIL_LENGTH) {
        trail.pop_back();
    }

    float newX = x + vx * deltaTime;
    float newY = y + vy * deltaTime;

    // Check for collision along the path (simple raycast)
    int steps = std::max(1, (int)(SPEED * deltaTime / 2.0f)); // Check every 2 pixels
    float stepX = (newX - x) / steps;
    float stepY = (newY - y) / steps;

    for (int i = 0; i <= steps; i++) {
        int checkX = (int)(x + stepX * i);
        int checkY = (int)(y + stepY * i);

        // Check for collision with scene objects
        for (auto& enemy : enemies) {
            if (enemy.isActive() && checkX >= enemy.getX() && checkX < enemy.getX() + enemy.getWidth() &&
                checkY >= enemy.getY() && checkY < enemy.getY() + enemy.getHeight()) {
                enemy.takeDamage(damage);
                active = false;
                return true;
            }
        }

        // Check for particle hit
        if (world.isOccupied(checkX, checkY)) {
            active = false;
            return true;
        }
    }

    x = newX;
    y = newY;
    return false;
}

// Draw bullet
void Bullet::draw(std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, SDL_Renderer* renderer) { // Removed scaleX, scaleY
    if (!active) return;

    // Calculate unscaled screen coordinates for the bullet's center
    int bulletScreenX = (int)(x - cameraX);
    int bulletScreenY = (int)(y - cameraY);

    // Draw the main bullet body (a pink square) directly into the pixels array
    const Uint32 pinkColor = 0xFF69B4; // Opaque pink (RGB)
    int bulletSize = 1; // Default pixel size for the bullet in the logical viewport

    for (int dy = -bulletSize; dy <= bulletSize; ++dy) {
        for (int dx = -bulletSize; dx <= bulletSize; ++dx) {
            int px = bulletScreenX + dx;
            int py = bulletScreenY + dy;
            if (px >= 0 && px < viewportWidth && py >= 0 && py < viewportHeight) {
                pixels[py * viewportWidth + px] = 0xFF000000 | pinkColor; // Add opaque alpha
            }
        }
    }

    // Draw the trail (simple fading dots) directly into the pixels array
    for (size_t i = 0; i < trail.size(); ++i) {
        float trailX = trail[i].first;
        float trailY = trail[i].second;

        // Calculate unscaled trail points
        int trailScreenX = (int)(trailX - cameraX);
        int trailScreenY = (int)(trailY - cameraY);

        if (trailScreenX >= 0 && trailScreenX < viewportWidth &&
            trailScreenY >= 0 && trailScreenY < viewportHeight) {

            // Fade out the trail by adjusting the alpha component of the pink color
            Uint8 alpha = 255 - (Uint8)(i * (255 / TRAIL_LENGTH));
            Uint32 fadedPink = (alpha << 24) | pinkColor; // ARGB format

            pixels[trailScreenY * viewportWidth + trailScreenX] = fadedPink;
        }
    }
}
