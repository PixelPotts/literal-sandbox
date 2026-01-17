#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <memory>
#include <vector>
#include <string>
#include "SceneObject.h"
#include "Sprite.h"

// Visual particle for explosion effect
struct ExplosionParticle {
    float x, y;
    float vx, vy;
    int r, g, b, a;
    float life;  // 0-1, decreases over time
};

class Collectible {
public:
    Collectible();
    ~Collectible();

    // Create from sprite file at position
    bool create(const std::string& spritePath, float x, float y, SDL_Renderer* renderer);

    // Attach a box collider (relative to position)
    void setCollider(float offsetX, float offsetY, float width, float height);

    // Check if player is touching and E is pressed - returns true if collected
    bool checkCollection(float playerX, float playerY, float playerW, float playerH, bool eKeyPressed);

    // Update explosion particles
    void update(float deltaTime);

    // Render the collectible or its explosion
    void render(SDL_Renderer* renderer, float cameraX, float cameraY, float scaleX, float scaleY);

    // Get the underlying scene object (for adding to world)
    std::shared_ptr<SceneObject> getSceneObject() const { return sceneObject; }

    // Check states
    bool isCollected() const { return collected; }
    bool isExplosionFinished() const { return collected && explosionParticles.empty(); }
    bool isActive() const { return !collected || !explosionParticles.empty(); }

    // Position getters
    float getX() const;
    float getY() const;

private:
    void triggerExplosion();
    void playSound();

    std::shared_ptr<SceneObject> sceneObject;
    std::shared_ptr<Sprite> sprite;

    // Collider (relative to position)
    float colliderX, colliderY, colliderW, colliderH;
    bool hasCollider;

    bool collected;
    bool wasInContact;  // For edge detection on E press

    // Explosion effect
    std::vector<ExplosionParticle> explosionParticles;

    // Sound
    static Mix_Chunk* bloopSound;
    static bool soundInitialized;
    static void initSound();
};
