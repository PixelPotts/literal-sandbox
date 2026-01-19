#include "LittlePurpleJumper.h"
#include <cmath>
#include <iostream>

LittlePurpleJumper::LittlePurpleJumper()
    : x(0), y(0)
    , velX(0), velY(0)
    , active(true)
    , isJumping(false)
    , jumpCooldown(0.0f)
    , onGround(false)
    , flipHorizontal(false)
    , hp(6), maxHp(6)
    , spriteSheet(nullptr)
    , world(nullptr)
{
}

void LittlePurpleJumper::takeDamage(int amount) {
    hp -= amount;
    if (hp <= 0) {
        hp = 0;
        active = false;
    }
}

void LittlePurpleJumper::renderHealthBar(SDL_Renderer* renderer, float cameraX, float cameraY, float scaleX, float scaleY) const {
    if (!active || hp <= 0 || hp == maxHp) return;

    float barWidth = 20.0f; // Width of the health bar in world units
    float barHeight = 2.0f; // Height of the health bar in world units
    float barYOffset = -5.0f; // Offset above the sprite

    float healthPercentage = (float)hp / (float)maxHp;

    // Background of the health bar (red)
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_Rect bgRect = {
        (int)((x - cameraX + (getWidth() - barWidth) / 2.0f) * scaleX),
        (int)((y - cameraY + barYOffset) * scaleY),
        (int)(barWidth * scaleX),
        (int)(barHeight * scaleY)
    };
    SDL_RenderFillRect(renderer, &bgRect);

    // Foreground of the health bar (green)
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect fgRect = {
        (int)((x - cameraX + (getWidth() - barWidth) / 2.0f) * scaleX),
        (int)((y - cameraY + barYOffset) * scaleY),
        (int)(barWidth * healthPercentage * scaleX),
        (int)(barHeight * scaleY)
    };
    SDL_RenderFillRect(renderer, &fgRect);
}

LittlePurpleJumper::~LittlePurpleJumper() {
    // spriteSheet and world are not owned, don't delete
}

void LittlePurpleJumper::init(float startX, float startY, MainSprite* mainSprite, World* aWorld) {
    x = startX;
    y = startY;
    spriteSheet = mainSprite;
    world = aWorld;
    active = true;
    isJumping = false;
    jumpCooldown = 0.0f;
    onGround = false;
    flipHorizontal = false;
    velX = 0;
    velY = 0;

    // Adjust initial Y position to ensure it starts on solid ground
    // First, check if the initial position is already in solid terrain. If so, move up.
    float capsuleCenterX = x + getWidth() / 2.0f;
    float currentY = y;
    float collisionY;
    int maxSearchUp = 10; // Max pixels to search up
    for(int i = 0; i < maxSearchUp; ++i) {
        if (world->checkCapsuleCollision(capsuleCenterX, currentY + collider_offsetY, collider_radius, collider_height, collisionY)) {
            currentY -= 1.0f; // Move up one pixel
        } else {
            break; // Found clear space
        }
    }
    y = currentY;

    // Now, let it fall until it hits ground
    float simulatedY = y;
    int maxSearchDown = 100; // Max pixels to search down for ground
    for (int i = 0; i < maxSearchDown; ++i) {
        float checkY = simulatedY + 1.0f; // Check 1 pixel below
        if (world->checkCapsuleCollision(capsuleCenterX, checkY + collider_offsetY, collider_radius, collider_height, collisionY)) {
            // Found ground, snap to it
            y = collisionY - (collider_offsetY + collider_height + collider_radius);
            onGround = true;
            break;
        }
        simulatedY += 1.0f;
        y = simulatedY; // Update y in case no ground is found within maxSearchDown
    }
}

void LittlePurpleJumper::update(float deltaTime, float playerX, float playerY) {
    if (!active || !world) return;

    // --- Ground Check ---
    float capsuleCenterX = x + getWidth() / 2.0f;
    float capsuleCheckY = y + collider_offsetY + 1; // Check 1px below
    float collisionY;
    onGround = world->checkCapsuleCollision(capsuleCenterX, capsuleCheckY, collider_radius, collider_height, collisionY);

    // --- Cooldown and Horizontal Physics (Friction) ---
    if (onGround) {
        velX *= GROUND_FRICTION;
        if (std::abs(velX) < 1.0f) {
            velX = 0;
        }
        if (jumpCooldown > 0) {
            jumpCooldown -= deltaTime;
        }
    }

    // --- Vertical Physics ---
    if (!onGround) {
        velY += GRAVITY * deltaTime;
    } else {
        velY = 0;
    }
    
    // Clamp fall speed
    if (velY > 400.0f) {
        velY = 400.0f;
    }

    // --- Proximity and Jumping ---
    float centerX = x + getWidth() / 2.0f;
    float centerY = y + getHeight() / 2.0f;
    float dx = playerX - centerX;
    float dy = playerY - centerY;
    float distance = std::sqrt(dx * dx + dy * dy);

    bool playerNear = (distance < TRIGGER_DISTANCE);

    // Trigger jump when player enters range, is on the ground, and cooldown is over
    if (playerNear && onGround && jumpCooldown <= 0) {
        isJumping = true;
        jumpCooldown = JUMP_COOLDOWN; // Reset cooldown
        velY = JUMP_VELOCITY_Y;
        
        // Set horizontal velocity to jump towards player
        float direction = (dx > 0) ? 1.0f : -1.0f;
        velX = JUMP_VELOCITY_X * direction;
        flipHorizontal = (velX > 0);

        onGround = false; // We are leaving the ground
    }
    
    // We are in the air if not on ground
    isJumping = !onGround;

    // --- Update Position ---
    float newX = x + velX * deltaTime;
    float newY = y + velY * deltaTime;
    
    // --- Collision Resolution ---
    // Y-axis collision
    float finalCapsuleCenterY = newY + collider_offsetY;
    if (world->checkCapsuleCollision(capsuleCenterX, finalCapsuleCenterY, collider_radius, collider_height, collisionY)) {
        if (velY > 0) { // Moving down
            y = collisionY - (collider_offsetY + collider_height + collider_radius);
            velY = 0;
            onGround = true;
        } else if (velY < 0) { // Moving up
            y = newY; 
            velY = 0; // Bonk head
        }
    } else {
        y = newY;
    }

    // X-axis collision (check at the final Y position)
    float finalCapsuleCenterX = newX + getWidth() / 2.0f;
    finalCapsuleCenterY = y + collider_offsetY;
    if (world->checkCapsuleCollision(finalCapsuleCenterX, finalCapsuleCenterY, collider_radius, collider_height, collisionY)) {
        // Hit a wall, stop horizontal movement
        velX = 0;
    } else {
        x = newX;
    }
}

void LittlePurpleJumper::render(SDL_Renderer* renderer, float cameraX, float cameraY,
                                 float scaleX, float scaleY) {
    if (!active || !spriteSheet) return;

    // Frame 0 = standing, Frame 1 = jumping
    int frame = isJumping ? 1 : 0;

    spriteSheet->renderFrame(renderer, "little_purple_jumper", frame,
                             x, y, cameraX, cameraY, scaleX, scaleY, flipHorizontal);
}
