#include "SparkBolt.h"
#include "BulletConfig.h"
#include "SpellModifier.h"
#include <algorithm>
#include <cmath>

SparkBolt::SparkBolt() {
    const auto& cfg = BulletConfigs::SparkBolt;
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

void SparkBolt::fire(World& world, float x, float y, float angle, int damage) {
    const auto& cfg = BulletConfigs::SparkBolt;

    for (int i = 0; i < projectileCount; ++i) {
        float spreadAngle = spread * ((float)rand() / RAND_MAX - 0.5f);
        float fireAngle = angle + spreadAngle;

        Bullet bullet(x, y, std::cos(fireAngle), std::sin(fireAngle), damage);
        bullet.applyConfig(cfg);

        // Apply modifiers
        for (auto& modifier : modifiers) {
            modifier->onFire(bullet);
        }

        bullets.push_back(bullet);
    }
}

void SparkBolt::update(float deltaTime, World& world, std::vector<LittlePurpleJumper>& enemies) {
    for (auto& bullet : bullets) {
        if (!bullet.active) continue;

        for (auto& modifier : modifiers) {
            modifier->onUpdate(bullet, world, deltaTime);
        }

        bullet.update(world, deltaTime, enemies);
    }

    cleanup();
}

void SparkBolt::render(SDL_Renderer* renderer, std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, float scaleX, float scaleY) {
    for (auto& bullet : bullets) {
        bullet.draw(pixels, viewportWidth, viewportHeight, cameraX, cameraY, renderer, spriteSheet);
    }
}

void SparkBolt::cleanup() {
    bullets.erase(
        std::remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& b) { return !b.active; }),
        bullets.end()
    );
}

int SparkBolt::getActiveBulletCount() const {
    int count = 0;
    for (const auto& bullet : bullets) {
        if (bullet.active) count++;
    }
    return count;
}
