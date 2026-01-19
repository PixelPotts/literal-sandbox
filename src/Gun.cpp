#include "Gun.h"
#include "LittlePurpleJumper.h"
#include <iostream>
#include <SDL.h> // For SDL_GetTicks()

Gun::Gun()
    : Collectible()
    , equipped(false)
    , pivotX(0)
    , pivotY(0)
    , angle(0)
    , flipped(false)
    , lastFireTime(0)
    , spriteWidth(0)
    , spriteHeight(0)
    , damage(3)
    , currentAmmunition(0)
{
    // Default wand stats (can be customized per gun instance)
    stats.name = "Basic Wand";
    stats.maxMana = 100;
    stats.currentMana = 100;
    stats.manaRechargeRate = 30.0f;
    stats.manaRechargeDelay = 0.3f;
    stats.castDelay = 0.15f;
    stats.rechargeTime = 0.3f;
    stats.speedMultiplier = 1.0f;
    stats.damageMultiplier = 1.0f;
    stats.capacity = 4;
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

bool Gun::fire(World& world) {
    if (ammunition.empty()) return false;

    // Check if we're in recharge state
    if (cycleComplete) return false;

    // Get current ammo and check mana cost
    auto& ammo = ammunition[currentAmmunition];
    int totalManaCost = ammo->manaCost;

    // Add modifier costs
    // (modifiers add to base cost through manaCostModifier property)

    if (stats.currentMana < totalManaCost) {
        // Not enough mana
        return false;
    }

    // Deduct mana
    stats.currentMana -= totalManaCost;
    timeSinceLastFire = 0.0f;  // Reset recharge delay timer

    float muzzleX, muzzleY;
    getMuzzlePosition(muzzleX, muzzleY);

    // Apply wand spread
    float wandSpread = stats.spreadDegrees * (3.14159f / 180.0f);
    float fireAngle = angle + wandSpread * ((float)rand() / RAND_MAX - 0.5f);

    // Apply wand damage multiplier
    int finalDamage = (int)(damage * stats.damageMultiplier);

    ammo->fire(world, muzzleX, muzzleY, fireAngle, finalDamage);
    lastFireTime = SDL_GetTicks();

    // Advance to next ammunition
    currentAmmunition++;
    if (currentAmmunition >= (int)ammunition.size()) {
        currentAmmunition = 0;
        cycleComplete = true;  // Start recharge
        rechargeTimer = 0.0f;
    }

    return true;
}

bool Gun::canFire(Uint32 currentTime) {
    // Can't fire while recharging the wand
    if (cycleComplete) {
        // std::cout << "canFire: false (recharging)" << std::endl;
        return false;
    }

    // Check cast delay
    Uint32 timeSinceLast = currentTime - lastFireTime;
    Uint32 castDelayMs = (Uint32)(stats.castDelay * 1000.0f);
    bool canFire = timeSinceLast >= castDelayMs;
    // if (!canFire) std::cout << "canFire: false (delay " << timeSinceLast << "/" << castDelayMs << ")" << std::endl;
    return canFire;
}

void Gun::update(float deltaTime) {
    if (!equipped) return;

    timeSinceLastFire += deltaTime;

    // Handle wand recharge (after cycling through all spells)
    if (cycleComplete) {
        rechargeTimer += deltaTime;
        if (rechargeTimer >= stats.rechargeTime) {
            cycleComplete = false;
            rechargeTimer = 0.0f;
        }
    }

    // Handle mana recharge (after delay)
    if (timeSinceLastFire >= stats.manaRechargeDelay && stats.currentMana < stats.maxMana) {
        manaRechargeAccumulator += stats.manaRechargeRate * deltaTime;
        if (manaRechargeAccumulator >= 1.0f) {
            int manaToAdd = (int)manaRechargeAccumulator;
            stats.currentMana += manaToAdd;
            manaRechargeAccumulator -= manaToAdd;
            if (stats.currentMana > stats.maxMana) {
                stats.currentMana = stats.maxMana;
                manaRechargeAccumulator = 0.0f;
            }
        }
    }
}

std::shared_ptr<Ammunition> Gun::getCurrentAmmunition() const {
    if (ammunition.empty()) return nullptr;
    return ammunition[currentAmmunition];
}

void Gun::addAmmunition(std::shared_ptr<Ammunition> ammo) {
    if (ammo && bulletSpriteSheet) {
        ammo->setSpriteSheet(bulletSpriteSheet);
    }
    ammunition.push_back(ammo);
}

void Gun::clearAmmunition() {
    ammunition.clear();
}

void Gun::updateAmmunition(float deltaTime, World& world, std::vector<LittlePurpleJumper>& enemies) {
    for (auto& ammo : ammunition) {
        ammo->update(deltaTime, world, enemies);
    }
}

void Gun::renderAmmunition(SDL_Renderer* renderer, std::vector<Uint32>& pixels, int viewportWidth, int viewportHeight, float cameraX, float cameraY, float scaleX, float scaleY) {
    for (auto& ammo : ammunition) {
        ammo->render(renderer, pixels, viewportWidth, viewportHeight, cameraX, cameraY, scaleX, scaleY);
    }
}
