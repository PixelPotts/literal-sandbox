#include "Gun.h"
#include <iostream>
#include <SDL.h> // For SDL_GetTicks()

Gun::Gun()
    : Collectible()
    , equipped(false)
    , pivotX(0)
    , pivotY(0)
    , angle(0)
    , flipped(false)
    , fireRate(5.0f) // 5 bullets per second
    , lastFireTime(0)
    , spriteWidth(0)
    , spriteHeight(0)
    , damage(3)
{
}

bool Gun::checkCollection(float playerX, float playerY, float playerW, float playerH, bool eKeyPressed) {
    if (equipped || isCollected()) return false;

    // Use parent's collision logic but don't explode
    auto sceneObj = getSceneObject();
    if (!sceneObj) return false;

    // Get collider bounds
    float myX = getX();
    float myY = getY();
    float myW = sceneObj->getSprite() ? sceneObj->getSprite()->getWidth() : 0;
    float myH = sceneObj->getSprite() ? sceneObj->getSprite()->getHeight() : 0;

    // AABB collision check
    bool inContact = (playerX < myX + myW &&
                      playerX + playerW > myX &&
                      playerY < myY + myH &&
                      playerY + playerH > myY);

    static bool wasInContact = false;

    if (inContact && eKeyPressed && !wasInContact) {
        // Equip the gun instead of exploding
        equipped = true;
        sceneObj->setVisible(false);

        // Cache sprite dimensions
        if (sceneObj->getSprite()) {
            spriteWidth = sceneObj->getSprite()->getWidth();
            spriteHeight = sceneObj->getSprite()->getHeight();
        }

        std::cout << "Gun equipped!" << std::endl;
        wasInContact = true;
        return true;
    }

    wasInContact = inContact && eKeyPressed;
    return false;
}

void Gun::updateEquipped(float playerCenterX, float playerCenterY, float cursorWorldX, float cursorWorldY) {
    if (!equipped) return;

    // Set pivot point at player center
    pivotX = playerCenterX;
    pivotY = playerCenterY;

    // Calculate angle from player center to cursor
    float dx = cursorWorldX - playerCenterX;
    float dy = cursorWorldY - playerCenterY;
    angle = std::atan2(dy, dx);

    // Flip sprite if pointing left (angle > 90 or < -90 degrees)
    flipped = (angle > M_PI / 2.0f || angle < -M_PI / 2.0f);
}

void Gun::renderEquipped(SDL_Renderer* renderer, float cameraX, float cameraY, float scaleX, float scaleY) {
    if (!equipped) return;

    auto sceneObj = getSceneObject();
    if (!sceneObj) return;

    Sprite* spr = sceneObj->getSprite();
    if (!spr || !spr->isLoaded()) return;

    SDL_Texture* tex = spr->getTexture();
    if (!tex) return;

    // Convert angle to degrees for SDL
    double angleDeg = angle * 180.0 / M_PI;

    // Screen position of pivot point
    int pivotScreenX = (int)((pivotX - cameraX) * scaleX);
    int pivotScreenY = (int)((pivotY - cameraY) * scaleY);

    // Sprite dimensions on screen
    int screenW = (int)(spriteWidth * scaleX);
    int screenH = (int)(spriteHeight * scaleY);

    // The pivot point (rotation center) is at the rear of the gun (left edge, vertical center)
    // SDL_RenderCopyEx rotates around the 'center' point we specify
    SDL_Point center;
    center.x = 0;  // Left edge of sprite
    center.y = screenH / 2;  // Vertical center

    // Destination rect - position so the pivot point ends up at player center
    SDL_Rect dstRect;
    dstRect.x = pivotScreenX;  // Left edge at pivot
    dstRect.y = pivotScreenY - screenH / 2;  // Center vertically on pivot
    dstRect.w = screenW;
    dstRect.h = screenH;

    // Flip mode for when pointing left
    SDL_RendererFlip flip = flipped ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE;

    // When flipped, adjust the angle
    double renderAngle = angleDeg;
    if (flipped) {
        renderAngle = angleDeg + 180.0;
    }

    SDL_RenderCopyEx(renderer, tex, nullptr, &dstRect, renderAngle, &center, flip);
}

void Gun::getMuzzlePosition(float& outX, float& outY) const {
    if (!equipped) {
        outX = pivotX;
        outY = pivotY;
        return;
    }

    // Muzzle is at the far end of the gun (right edge when not flipped)
    float muzzleOffset = (float)spriteWidth;

    outX = pivotX + std::cos(angle) * muzzleOffset;
    outY = pivotY + std::sin(angle) * muzzleOffset;
}

void Gun::fire(std::vector<Bullet>& bullets, float startX, float startY, float targetX, float targetY) {
    bullets.emplace_back(startX, startY, targetX, targetY, damage);
    lastFireTime = SDL_GetTicks(); // Update last fire time internally
}

bool Gun::canFire(Uint32 currentTime) {
    Uint32 timeSinceLastFire = currentTime - lastFireTime;
    Uint32 fireDelay = (Uint32)(1000.0f / fireRate);
    return timeSinceLastFire >= fireDelay;
}
