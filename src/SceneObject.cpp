#include "SceneObject.h"
#include <iostream>

SceneObject::SceneObject()
    : x(0), y(0)
    , velX(0), velY(0)
    , collider{0, 0, 0, 0}
    , capsule{0, 0, 0, 0}
    , colliderEnabled(false)
    , capsuleEnabled(false)
    , active(true)
    , visible(true)
    , staticObject(false)
    , blockParticles(false)
    , hp(10), maxHp(10)
{
}

void SceneObject::takeDamage(int amount) {
    hp -= amount;
    if (hp < 0) {
        hp = 0;
    }
    std::cout << "Object took " << amount << " damage, " << hp << " hp remaining." << std::endl;
}

void SceneObject::setCollider(float cx, float cy, float cw, float ch) {
    collider.x = cx;
    collider.y = cy;
    collider.width = cw;
    collider.height = ch;
    colliderEnabled = true;
}

bool SceneObject::collidesWith(const SceneObject& other) const {
    if (!colliderEnabled || !other.colliderEnabled) return false;
    return collider.intersects(other.collider, x, y, other.x, other.y);
}

bool SceneObject::containsPoint(float px, float py) const {
    if (!colliderEnabled) return false;

    float cx = x + collider.x;
    float cy = y + collider.y;

    return px >= cx && px < cx + collider.width &&
           py >= cy && py < cy + collider.height;
}

bool SceneObject::isPixelSolidAt(int worldX, int worldY) const {
    if (!sprite || !sprite->isLoaded()) return false;

    // Convert world position to sprite-local position
    int localX = worldX - (int)x;
    int localY = worldY - (int)y;

    return sprite->isPixelSolid(localX, localY);
}

void SceneObject::update(float deltaTime) {
    if (staticObject) return;

    // Apply velocity
    x += velX * deltaTime;
    y += velY * deltaTime;
}

void SceneObject::getWorldCollider(float& outX, float& outY, float& outW, float& outH) const {
    outX = x + collider.x;
    outY = y + collider.y;
    outW = collider.width;
    outH = collider.height;
}

void SceneObject::setCapsuleCollider(float radius, float height) {
    // Center the capsule on the sprite
    capsule.offsetX = (sprite ? sprite->getWidth() / 2.0f : 0);
    capsule.offsetY = (sprite ? sprite->getHeight() / 2.0f : height / 2.0f);
    capsule.radius = radius;
    capsule.height = height;
    capsuleEnabled = true;
}

void SceneObject::setCapsuleCollider(float offsetX, float offsetY, float radius, float height) {
    capsule.offsetX = offsetX;
    capsule.offsetY = offsetY;
    capsule.radius = radius;
    capsule.height = height;
    capsuleEnabled = true;
}

void SceneObject::renderHealthBar(SDL_Renderer* renderer, float cameraX, float cameraY, float scaleX, float scaleY) const {
    if (!sprite || hp <= 0 || hp == maxHp) return;

    float barWidth = 20.0f; // Width of the health bar in world units
    float barHeight = 2.0f; // Height of the health bar in world units
    float barYOffset = -5.0f; // Offset above the sprite

    float healthPercentage = (float)hp / (float)maxHp;

    // Background of the health bar (red)
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_Rect bgRect = {
        (int)((x - cameraX + (sprite->getWidth() - barWidth) / 2.0f) * scaleX),
        (int)((y - cameraY + barYOffset) * scaleY),
        (int)(barWidth * scaleX),
        (int)(barHeight * scaleY)
    };
    SDL_RenderFillRect(renderer, &bgRect);

    // Foreground of the health bar (green)
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect fgRect = {
        (int)((x - cameraX + (sprite->getWidth() - barWidth) / 2.0f) * scaleX),
        (int)((y - cameraY + barYOffset) * scaleY),
        (int)(barWidth * healthPercentage * scaleX),
        (int)(barHeight * scaleY)
    };
    SDL_RenderFillRect(renderer, &fgRect);
}
