#pragma once
#include "Collectible.h"
#include "Bullet.h" // Include Bullet.h
#include <cmath>

class Gun : public Collectible {
public:
    Gun();
    ~Gun() = default;

    // Override checkCollection to attach to player instead of exploding
    bool checkCollection(float playerX, float playerY, float playerW, float playerH, bool eKeyPressed);

    // Update gun state (call each frame when equipped)
    void updateEquipped(float playerCenterX, float playerCenterY, float cursorWorldX, float cursorWorldY);

    // Render the gun when equipped (with rotation toward cursor)
    void renderEquipped(SDL_Renderer* renderer, float cameraX, float cameraY, float scaleX, float scaleY);

    // Check if gun is equipped
    bool isEquipped() const { return equipped; }

    // Get current angle (radians)
    float getAngle() const { return angle; }

    // Get muzzle position (world coordinates) - for bullet spawning
    void getMuzzlePosition(float& outX, float& outY) const;

    // Check if gun is flipped (pointing left)
    bool isFlipped() const { return flipped; }

    // Fire method
    void fire(std::vector<Bullet>& bullets, float startX, float startY, float targetX, float targetY);

    // Check if the gun can fire based on fire rate
    bool canFire(Uint32 currentTime);

private:
    bool equipped;
    float pivotX, pivotY;    // World position of pivot point (at player center)
    float angle;             // Rotation angle in radians
    bool flipped;            // True if pointing left (flip sprite vertically)

    // Gun firing properties
    float fireRate;          // Bullets per second
    Uint32 lastFireTime;     // SDL_GetTicks() at last fire

    // Sprite dimensions (cached after load)
    int spriteWidth;
    int spriteHeight;
};
