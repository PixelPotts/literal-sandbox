#include "BouncingBolt.h"
#include "BulletConfig.h"
#include "SpellModifier.h"
#include <algorithm>
#include <cmath>

BouncingBolt::BouncingBolt() {
    const auto& cfg = BulletConfigs::BouncingBolt;
    name = cfg.name;
    description = cfg.description;
    manaCost = cfg.manaCost;
    spread = cfg.spread;
    lifetime = cfg.lifetime;
    projectileColor = cfg.color;
    spriteRegion = cfg.sprite;
    animated = cfg.animated;
    frameCount = cfg.frameCount;
    frameTime = cfg.frameTime;
    leavesTrail = cfg.leavesTrail;
    trailParticleType = cfg.trailParticleType;
    trailInterval = cfg.trailInterval;
}

void BouncingBolt::fire(World& world, float x, float y, float angle, int damage) {
    const auto& cfg = BulletConfigs::BouncingBolt;

    for (int i = 0; i < projectileCount; ++i) {
        float spreadAngle = 0.0f;
        if (projectileCount > 1) {
            float totalSpread = spread * 2.0f;
            spreadAngle = -spread + (totalSpread * i / (projectileCount - 1));
        } else {
            spreadAngle = spread * ((float)rand() / RAND_MAX - 0.5f);
        }

        float fireAngle = angle + spreadAngle;
        Bullet bullet(x, y, std::cos(fireAngle), std::sin(fireAngle), damage);
        bullet.applyConfig(cfg);

        // Apply all modifiers
        for (auto& modifier : modifiers) {
            modifier->onFire(bullet);
        }

        bullets.push_back(bullet);
    }
}

void BouncingBolt::update(float deltaTime, World& world, std::vector<LittlePurpleJumper>& enemies) {
    for (auto& bullet : bullets) {
        if (!bullet.active) continue;

        // Apply modifier updates
        for (auto& modifier : modifiers) {
            modifier->onUpdate(bullet, world, deltaTime);
        }

        bullet.update(world, deltaTime, enemies);
    }

    // Periodic cleanup
    cleanup();
}

void BouncingBolt::render(SDL_Renderer* renderer, std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, float scaleX, float scaleY) {
    for (auto& bullet : bullets) {
        bullet.draw(pixels, viewportWidth, viewportHeight, cameraX, cameraY, renderer, spriteSheet);
    }
}

void BouncingBolt::cleanup() {
    bullets.erase(
        std::remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& b) { return !b.active; }),
        bullets.end()
    );
}

int BouncingBolt::getActiveBulletCount() const {
    int count = 0;
    for (const auto& bullet : bullets) {
        if (bullet.active) count++;
    }
    return count;
}
