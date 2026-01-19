#include "FireBolt.h"
#include "BulletConfig.h"
#include "SpellModifier.h"
#include <algorithm>
#include <cmath>

FireBolt::FireBolt() {
    const auto& cfg = BulletConfigs::FireBolt;
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

void FireBolt::fire(World& world, float x, float y, float angle, int damage) {
    const auto& cfg = BulletConfigs::FireBolt;

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

void FireBolt::update(float deltaTime, World& world, std::vector<LittlePurpleJumper>& enemies) {
    fireSpawnTimer += deltaTime;

    for (auto& bullet : bullets) {
        if (!bullet.active) continue;

        // Store position before update
        float oldX = bullet.x;
        float oldY = bullet.y;

        for (auto& modifier : modifiers) {
            modifier->onUpdate(bullet, world, deltaTime);
        }

        bullet.update(world, deltaTime, enemies);

        // Leave fire trail at OLD position (where bullet was, not where it is now)
        // This prevents the bullet from colliding with its own trail
        if (bullet.active && fireSpawnTimer >= 0.03f) {
            fireSpawnTimer = 0.0f;
            int wx = (int)oldX;
            int wy = (int)oldY;
            world.spawnParticleAt(wx, wy, ParticleType::FIRE);
        }
    }

    cleanup();
}

void FireBolt::render(SDL_Renderer* renderer, std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, float scaleX, float scaleY) {
    for (auto& bullet : bullets) {
        bullet.draw(pixels, viewportWidth, viewportHeight, cameraX, cameraY, renderer, spriteSheet);
    }
}

void FireBolt::cleanup() {
    bullets.erase(
        std::remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& b) { return !b.active; }),
        bullets.end()
    );
}

int FireBolt::getActiveBulletCount() const {
    int count = 0;
    for (const auto& bullet : bullets) {
        if (bullet.active) count++;
    }
    return count;
}
