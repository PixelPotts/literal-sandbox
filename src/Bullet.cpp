#include "Bullet.h"
#include "BulletConfig.h"
#include "Sprite.h"
#include "LittlePurpleJumper.h"
#include <SDL.h>
#include <iostream>

void Bullet::applyConfig(const BulletTypeConfig& config) {
    vx *= config.speed;
    vy *= config.speed;
    lifetime = config.lifetime;
    damage += config.damage;
    bouncesRemaining = config.bounces;
    piercesRemaining = config.pierces;
    homingStrength = config.homingStrength;
    homingRange = config.homingRange;
    color = config.color;
    spriteRegion = config.sprite;
    animated = config.animated;
    frameCount = config.frameCount;
    frameTime = config.frameTime;
}

bool Bullet::update(World& world, float deltaTime, std::vector<LittlePurpleJumper>& enemies) {
    if (!active) return false;

    // Update lifetime
    lifetime -= deltaTime;
    if (lifetime <= 0) {
        active = false;
        return false;
    }

    // Update animation
    if (animated && frameCount > 1) {
        animTimer += deltaTime;
        if (animTimer >= frameTime) {
            animTimer -= frameTime;
            currentFrame = (currentFrame + 1) % frameCount;
        }
    }

    // Store current position for trail
    trail.insert(trail.begin(), {x, y});
    if (trail.size() > TRAIL_LENGTH) {
        trail.pop_back();
    }

    // Apply homing if enabled
    if (homingStrength > 0 && homingRange > 0) {
        float closestDist = homingRange;
        float targetX = 0, targetY = 0;
        bool foundTarget = false;

        for (auto& enemy : enemies) {
            if (!enemy.isActive()) continue;
            float ex = enemy.getX() + enemy.getWidth() / 2.0f;
            float ey = enemy.getY() + enemy.getHeight() / 2.0f;
            float dx = ex - x;
            float dy = ey - y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < closestDist) {
                closestDist = dist;
                targetX = ex;
                targetY = ey;
                foundTarget = true;
            }
        }

        if (foundTarget) {
            float dx = targetX - x;
            float dy = targetY - y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 0) {
                float homingX = (dx / dist) * homingStrength * deltaTime;
                float homingY = (dy / dist) * homingStrength * deltaTime;
                vx += homingX;
                vy += homingY;
                float currentSpeed = std::sqrt(vx * vx + vy * vy);
                float targetSpeed = SPEED;
                if (currentSpeed > 0) {
                    vx = (vx / currentSpeed) * targetSpeed;
                    vy = (vy / currentSpeed) * targetSpeed;
                }
            }
        }
    }

    // Update angle based on velocity direction
    if (vx != 0 || vy != 0) {
        angle = std::atan2(vy, vx);
    }

    float newX = x + vx * deltaTime;
    float newY = y + vy * deltaTime;

    // Check for collision along the path (simple raycast)
    float currentSpeed = std::sqrt(vx * vx + vy * vy);
    int steps = std::max(1, (int)(currentSpeed * deltaTime / 2.0f)); // Check every 2 pixels
    float stepX = (newX - x) / steps;
    float stepY = (newY - y) / steps;

    // Start from i=1 to skip collision check at starting position
    // This prevents bullets from colliding with trail particles spawned at their current pos
    for (int i = 1; i <= steps; i++) {
        int checkX = (int)(x + stepX * i);
        int checkY = (int)(y + stepY * i);

        // Check for collision with scene objects (enemies)
        for (auto& enemy : enemies) {
            if (enemy.isActive() && checkX >= enemy.getX() && checkX < enemy.getX() + enemy.getWidth() &&
                checkY >= enemy.getY() && checkY < enemy.getY() + enemy.getHeight()) {
                enemy.takeDamage(damage);

                // Handle piercing
                if (piercesRemaining > 0) {
                    piercesRemaining--;
                    // Continue through enemy, don't deactivate
                    continue;
                }

                active = false;
                return true;
            }
        }

        // Check for particle hit
        if (world.isOccupied(checkX, checkY)) {
            // Handle bouncing
            if (bouncesRemaining > 0) {
                bouncesRemaining--;

                // Determine bounce direction by checking adjacent cells
                bool solidLeft = world.isOccupied(checkX - 1, checkY);
                bool solidRight = world.isOccupied(checkX + 1, checkY);
                bool solidUp = world.isOccupied(checkX, checkY - 1);
                bool solidDown = world.isOccupied(checkX, checkY + 1);

                // Reverse appropriate velocity component
                if ((solidLeft && vx < 0) || (solidRight && vx > 0)) {
                    vx = -vx;
                }
                if ((solidUp && vy < 0) || (solidDown && vy > 0)) {
                    vy = -vy;
                }

                // If can't determine direction, just reverse both
                if (!solidLeft && !solidRight && !solidUp && !solidDown) {
                    vx = -vx;
                    vy = -vy;
                }

                // Move back slightly to avoid getting stuck
                x = x + stepX * (i - 1);
                y = y + stepY * (i - 1);
                return false;
            }

            active = false;
            return true;
        }
    }

    x = newX;
    y = newY;
    return false;
}

void Bullet::draw(std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight,
                  float cameraX, float cameraY, SDL_Renderer* renderer, Sprite* spriteSheet) {
    if (!active) return;

    int bulletScreenX = (int)(x - cameraX);
    int bulletScreenY = (int)(y - cameraY);

    Uint32 bulletColor = color;
    if (isCritical) {
        bulletColor = 0xFFD700;  // Gold for crits
    }

    // Draw trail first (behind bullet)
    for (size_t i = 0; i < trail.size(); ++i) {
        float trailX = trail[i].first;
        float trailY = trail[i].second;

        int trailScreenX = (int)(trailX - cameraX);
        int trailScreenY = (int)(trailY - cameraY);

        if (trailScreenX >= 0 && trailScreenX < viewportWidth &&
            trailScreenY >= 0 && trailScreenY < viewportHeight) {

            float fade = 1.0f - ((float)i / TRAIL_LENGTH);
            Uint8 r = (Uint8)(((bulletColor >> 16) & 0xFF) * fade);
            Uint8 g = (Uint8)(((bulletColor >> 8) & 0xFF) * fade);
            Uint8 b = (Uint8)((bulletColor & 0xFF) * fade);
            Uint32 fadedColor = 0xFF000000 | (r << 16) | (g << 8) | b;

            pixels[trailScreenY * viewportWidth + trailScreenX] = fadedColor;
        }
    }

    // Check if we have a valid sprite region
    bool hasSprite = spriteSheet && spriteSheet->isLoaded() &&
                     spriteRegion.width > 0 && spriteRegion.height > 0;

    if (hasSprite && renderer) {
        // Render sprite with rotation
        SDL_Texture* tex = spriteSheet->getTexture();
        if (tex) {
            // Source rectangle (sprite region, with animation frame offset)
            SDL_Rect srcRect;
            srcRect.x = spriteRegion.x + (currentFrame * spriteRegion.width);
            srcRect.y = spriteRegion.y;
            srcRect.w = spriteRegion.width;
            srcRect.h = spriteRegion.height;

            // Destination rectangle (centered on bullet position)
            SDL_Rect dstRect;
            dstRect.x = bulletScreenX - spriteRegion.width / 2;
            dstRect.y = bulletScreenY - spriteRegion.height / 2;
            dstRect.w = spriteRegion.width;
            dstRect.h = spriteRegion.height;

            // Convert angle to degrees
            double angleDeg = angle * 180.0 / M_PI;

            // Rotation center (middle of sprite)
            SDL_Point center = {spriteRegion.width / 2, spriteRegion.height / 2};

            // Apply crit tint
            if (isCritical) {
                SDL_SetTextureColorMod(tex, 255, 215, 0);  // Gold tint
            }

            SDL_RenderCopyEx(renderer, tex, &srcRect, &dstRect, angleDeg, &center, SDL_FLIP_NONE);

            // Reset tint
            if (isCritical) {
                SDL_SetTextureColorMod(tex, 255, 255, 255);
            }
        }
    } else {
        // Fallback: draw colored pixels
        int bulletSize = 1 + (damage / 10);
        if (bulletSize > 3) bulletSize = 3;

        for (int dy = -bulletSize; dy <= bulletSize; ++dy) {
            for (int dx = -bulletSize; dx <= bulletSize; ++dx) {
                int px = bulletScreenX + dx;
                int py = bulletScreenY + dy;
                if (px >= 0 && px < viewportWidth && py >= 0 && py < viewportHeight) {
                    pixels[py * viewportWidth + px] = 0xFF000000 | bulletColor;
                }
            }
        }
    }
}
