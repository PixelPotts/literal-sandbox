#pragma once
#include "Sprite.h"
#include <memory>
#include <vector>
#include <cmath>

// Local bounding box for scene object collision (relative coords)
struct BoundingBox {
    float x, y;          // Top-left position (relative to object)
    float width, height;

    bool intersects(const BoundingBox& other, float thisX, float thisY, float otherX, float otherY) const {
        float ax = thisX + x;
        float ay = thisY + y;
        float bx = otherX + other.x;
        float by = otherY + other.y;

        return ax < bx + other.width &&
               ax + width > bx &&
               ay < by + other.height &&
               ay + height > by;
    }
};

// Capsule collider for character physics
struct CharacterCapsule {
    float offsetX, offsetY;  // Offset from object position
    float radius;            // Radius of the capsule ends
    float height;            // Total height (including caps)

    // Get center position in world coordinates
    void getWorldCenter(float objX, float objY, float& cx, float& cy) const {
        cx = objX + offsetX;
        cy = objY + offsetY;
    }

    // Check if a point is inside the capsule (world coordinates)
    bool containsPoint(float objX, float objY, float px, float py) const {
        float cx, cy;
        getWorldCenter(objX, objY, cx, cy);

        float halfH = (height - 2 * radius) / 2;
        float topY = cy - halfH;
        float botY = cy + halfH;

        // Check middle rectangle
        if (px >= cx - radius && px <= cx + radius && py >= topY && py <= botY) {
            return true;
        }

        // Check top circle
        float dx = px - cx;
        float dy = py - topY;
        if (dx*dx + dy*dy <= radius*radius) return true;

        // Check bottom circle
        dy = py - botY;
        if (dx*dx + dy*dy <= radius*radius) return true;

        return false;
    }
};

// Base class for non-particle scene objects
class SceneObject {
public:
    SceneObject();
    virtual ~SceneObject() = default;

    // Position in world coordinates
    float getX() const { return x; }
    float getY() const { return y; }
    void setPosition(float newX, float newY) { x = newX; y = newY; }
    void move(float dx, float dy) { x += dx; y += dy; }

    // Velocity
    float getVelX() const { return velX; }
    float getVelY() const { return velY; }
    void setVelocity(float vx, float vy) { velX = vx; velY = vy; }

    // Sprite
    void setSprite(std::shared_ptr<Sprite> spr) { sprite = spr; }
    Sprite* getSprite() { return sprite.get(); }
    const Sprite* getSprite() const { return sprite.get(); }

    // Box collider (relative to position)
    void setCollider(float cx, float cy, float cw, float ch);
    const BoundingBox& getCollider() const { return collider; }
    bool hasCollider() const { return colliderEnabled; }
    void enableCollider(bool enable) { colliderEnabled = enable; }

    // Capsule collider for character physics
    void setCapsuleCollider(float radius, float height);
    void setCapsuleCollider(float offsetX, float offsetY, float radius, float height);
    const CharacterCapsule& getCapsule() const { return capsule; }
    bool hasCapsule() const { return capsuleEnabled; }
    void enableCapsule(bool enable) { capsuleEnabled = enable; }

    // Check collision with another object
    bool collidesWith(const SceneObject& other) const;

    // Check if a world point is inside this object's collider
    bool containsPoint(float px, float py) const;

    // Check pixel-perfect collision at a world position
    bool isPixelSolidAt(int worldX, int worldY) const;

    // Properties
    bool isActive() const { return active; }
    void setActive(bool act) { active = act; }

    bool isVisible() const { return visible; }
    void setVisible(bool vis) { visible = vis; }

    bool isStatic() const { return staticObject; }
    void setStatic(bool stat) { staticObject = stat; }

    // For particle interaction - does this object block particles?
    bool blocksParticles() const { return blockParticles; }
    void setBlocksParticles(bool block) { blockParticles = block; }

    // Update (override in derived classes)
    virtual void update(float deltaTime);

    // Get world-space collider bounds
    void getWorldCollider(float& outX, float& outY, float& outW, float& outH) const;

protected:
    float x, y;              // World position
    float velX, velY;        // Velocity
    std::shared_ptr<Sprite> sprite;
    BoundingBox collider;
    CharacterCapsule capsule;
    bool colliderEnabled;
    bool capsuleEnabled;
    bool active;
    bool visible;
    bool staticObject;       // Static objects don't move
    bool blockParticles;     // Does this block particle movement?
};
