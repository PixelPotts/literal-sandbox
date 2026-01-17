#include "Collectible.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <algorithm>

// Static members
Mix_Chunk* Collectible::bloopSound = nullptr;
bool Collectible::soundInitialized = false;

void Collectible::initSound() {
    if (soundInitialized) return;

    // Initialize SDL_mixer if not already
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "SDL_mixer could not initialize: " << Mix_GetError() << std::endl;
    }

    // Try to load bloop sound
    bloopSound = Mix_LoadWAV("../sounds/bloop.wav");
    if (!bloopSound) {
        // Try alternate path
        bloopSound = Mix_LoadWAV("sounds/bloop.wav");
    }
    if (!bloopSound) {
        std::cerr << "Could not load bloop sound: " << Mix_GetError() << std::endl;
    }

    soundInitialized = true;
}

Collectible::Collectible()
    : colliderX(0), colliderY(0), colliderW(0), colliderH(0)
    , hasCollider(false)
    , collected(false)
    , wasInContact(false)
{
    initSound();
}

Collectible::~Collectible() {
    // Sound is static, don't free here
}

bool Collectible::create(const std::string& spritePath, float x, float y, SDL_Renderer* renderer) {
    sprite = std::make_shared<Sprite>();
    if (!sprite->load(spritePath, renderer)) {
        std::cerr << "Collectible: Failed to load sprite: " << spritePath << std::endl;
        return false;
    }

    sceneObject = std::make_shared<SceneObject>();
    sceneObject->setSprite(sprite);
    sceneObject->setPosition(x, y);

    // Default collider covers entire sprite
    colliderX = 0;
    colliderY = 0;
    colliderW = (float)sprite->getWidth();
    colliderH = (float)sprite->getHeight();
    hasCollider = true;

    return true;
}

void Collectible::setCollider(float offsetX, float offsetY, float width, float height) {
    colliderX = offsetX;
    colliderY = offsetY;
    colliderW = width;
    colliderH = height;
    hasCollider = true;
}

float Collectible::getX() const {
    return sceneObject ? sceneObject->getX() : 0;
}

float Collectible::getY() const {
    return sceneObject ? sceneObject->getY() : 0;
}

bool Collectible::checkCollection(float playerX, float playerY, float playerW, float playerH, bool eKeyPressed) {
    if (collected || !hasCollider || !sceneObject) return false;

    // Get world-space collider bounds
    float myX = sceneObject->getX() + colliderX;
    float myY = sceneObject->getY() + colliderY;

    // AABB collision check
    bool inContact = (playerX < myX + colliderW &&
                      playerX + playerW > myX &&
                      playerY < myY + colliderH &&
                      playerY + playerH > myY);

    if (inContact && eKeyPressed && !wasInContact) {
        // Collect!
        collected = true;
        sceneObject->setVisible(false);
        triggerExplosion();
        playSound();
        return true;
    }

    wasInContact = inContact && eKeyPressed;
    return false;
}

void Collectible::triggerExplosion() {
    if (!sprite) return;

    int w = sprite->getWidth();
    int h = sprite->getHeight();
    float centerX = getX() + w / 2.0f;
    float centerY = getY() + h / 2.0f;

    // Create particles from sprite pixels
    for (int py = 0; py < h; py += 2) {  // Skip every other pixel for performance
        for (int px = 0; px < w; px += 2) {
            unsigned char r, g, b, a;
            sprite->getPixelColor(px, py, r, g, b, a);

            if (a > 128) {  // Only visible pixels
                ExplosionParticle p;
                p.x = getX() + px;
                p.y = getY() + py;

                // Explode outward from center
                float dx = p.x - centerX;
                float dy = p.y - centerY;
                float dist = std::sqrt(dx * dx + dy * dy) + 0.1f;
                float speed = 50.0f + (rand() % 100);

                p.vx = (dx / dist) * speed + (rand() % 40 - 20);
                p.vy = (dy / dist) * speed + (rand() % 40 - 20) - 30.0f;  // Slight upward bias

                p.r = r;
                p.g = g;
                p.b = b;
                p.a = a;
                p.life = 1.0f;

                explosionParticles.push_back(p);
            }
        }
    }
}

void Collectible::playSound() {
    if (bloopSound) {
        Mix_VolumeChunk(bloopSound, MIX_MAX_VOLUME / 3);  // Soft volume
        Mix_PlayChannel(-1, bloopSound, 0);
    }
}

void Collectible::update(float deltaTime) {
    // Update explosion particles
    for (auto& p : explosionParticles) {
        p.x += p.vx * deltaTime;
        p.y += p.vy * deltaTime;
        p.vy += 150.0f * deltaTime;  // Gravity
        p.life -= deltaTime * 1.5f;  // Fade out
        p.a = (int)(p.life * 255);
    }

    // Remove dead particles
    explosionParticles.erase(
        std::remove_if(explosionParticles.begin(), explosionParticles.end(),
            [](const ExplosionParticle& p) { return p.life <= 0; }),
        explosionParticles.end()
    );
}

void Collectible::render(SDL_Renderer* renderer, float cameraX, float cameraY, float scaleX, float scaleY) {
    // Render explosion particles
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (const auto& p : explosionParticles) {
        int screenX = (int)((p.x - cameraX) * scaleX);
        int screenY = (int)((p.y - cameraY) * scaleY);
        int size = (int)(2 * scaleX);  // 2 pixel particles

        SDL_SetRenderDrawColor(renderer, p.r, p.g, p.b, (Uint8)std::max(0, std::min(255, p.a)));
        SDL_Rect rect = {screenX, screenY, size, size};
        SDL_RenderFillRect(renderer, &rect);
    }
}
