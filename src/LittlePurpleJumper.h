#pragma once
#include "MainSprite.h"
#include "World.h" // Include World header for collision
#include <memory>

class LittlePurpleJumper {
public:
    LittlePurpleJumper();
    ~LittlePurpleJumper();

    // Initialize at a position with reference to shared sprite sheet and world
    void init(float x, float y, MainSprite* mainSprite, World* world);

    // Update logic - pass player position for proximity detection
    void update(float deltaTime, float playerX, float playerY);

    // Render using the main sprite sheet
    void render(SDL_Renderer* renderer, float cameraX, float cameraY, float scaleX, float scaleY);

    // Position
    float getX() const { return x; }
    float getY() const { return y; }
    void setPosition(float newX, float newY) { x = newX; y = newY; }

    // Get sprite dimensions
    int getWidth() const { return 5; }
    int getHeight() const { return 16; }

    // Check if active
    bool isActive() const { return active; }
    void setActive(bool a) { active = a; }

    // Health and damage
    void takeDamage(int amount);
    void renderHealthBar(SDL_Renderer* renderer, float cameraX, float cameraY, float scaleX, float scaleY) const;

private:
    float x, y;              // World position
    float velX, velY;        // Velocities
    bool active;
    bool isJumping;
    float jumpCooldown;      // Timer for pausing between jumps
    bool onGround;
    bool flipHorizontal;

    int hp;
    int maxHp;

    MainSprite* spriteSheet; // Shared reference, not owned
    World* world;            // Shared reference for collision

    // Collider properties (capsule shape)
    float collider_radius = 2.5f;
    float collider_height = 3.0f;
    float collider_offsetY = 5.0f; // Offset from y to start of capsule

    static constexpr float JUMP_COOLDOWN = 3.0f; // 3 seconds
    static constexpr float JUMP_VELOCITY_Y = -120.0f;
    static constexpr float JUMP_VELOCITY_X = 50.0f;
    static constexpr float GRAVITY = 400.0f;
    static constexpr float GROUND_FRICTION = 0.9f;
    static constexpr float TRIGGER_DISTANCE = 150.0f;
};
