#include "Bullet.h"
#include "SDL.h" // For Uint32
#include <iostream>

// Update bullet position, return true if hit something
bool Bullet::update(World& world, float deltaTime) {
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

        // Check world bounds
        if (!world.inWorldBounds(checkX, checkY)) {
            active = false;
            return false;
        }

        // Check for particle hit
        if (world.isOccupied(checkX, checkY)) {
            // Explode at this position
            world.explodeAt(checkX, checkY, EXPLOSION_RADIUS, EXPLOSION_FORCE);
            active = false;
            return true;
        }
    }

    x = newX;
    y = newY;
    return false;
}

// Draw bullet
void Bullet::draw(std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, SDL_Renderer* renderer) {
    if (!active) return;

    int screenX = (int)(x - cameraX);
    int screenY = (int)(y - cameraY);

    // Draw the purple cross (5x5)
    // R: 128 (80), G: 0 (00), B: 128 (80)
    const Uint32 purple = 0x800080; // A darker purple
    for (int i = -2; i <= 2; i++) {
        // Horizontal line
        int px = screenX + i;
        int py = screenY;
        if (px >= 0 && px < viewportWidth && py >= 0 && py < viewportHeight) {
            pixels[py * viewportWidth + px] = purple;
        }
        // Vertical line
        px = screenX;
        py = screenY + i;
        if (px >= 0 && px < viewportWidth && py >= 0 && py < viewportHeight) {
            pixels[py * viewportWidth + px] = purple;
        }
    }

    // Draw the trail (simple fading dots for now)
    // To implement the "sin wave-y purple dust magic", I'll need a more advanced particle system or
    // custom drawing logic, potentially using SDL_RenderDrawPoint with alpha blending.
    // For now, I'll draw fading dots.
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); // Enable blending for alpha
    for (size_t i = 0; i < trail.size(); ++i) {
        float trailX = trail[i].first;
        float trailY = trail[i].second;

        int trailScreenX = (int)(trailX - cameraX);
        int trailScreenY = (int)(trailY - cameraY);

        if (trailScreenX >= 0 && trailScreenX < viewportWidth &&
            trailScreenY >= 0 && trailScreenY < viewportHeight) {

            // Fade out the trail
            Uint8 alpha = 255 - (Uint8)(i * (255 / TRAIL_LENGTH));
            SDL_SetRenderDrawColor(renderer, 0x80, 0x00, 0x80, alpha); // Purple with fading alpha
            SDL_RenderDrawPoint(renderer, trailScreenX, trailScreenY);
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE); // Disable blending
}
