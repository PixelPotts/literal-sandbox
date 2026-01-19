#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_opengl.h>
#include <iostream>
#include <string>
#include <cmath>
#include <vector>
#include <algorithm>
#include "Config.h"
#include "World.h"
#include "SandSimulator.h"
#include "Sprite.h"
#include "SceneObject.h"
#include "Collectible.h"
#include "Gun.h"
#include "Bullet.h"
#include "ZLayers.h"
#include "MainSprite.h"
#include "LittlePurpleJumper.h"
#include "BouncingBolt.h"
#include "SparkBolt.h"
#include "FireBolt.h"
#include "MagicMissile.h"
#include "SpellModifier.h"

struct UIDropdown {
    SDL_Rect rect;
    bool isOpen;
    int selectedIndex;
    std::vector<std::string> options;
    std::vector<ParticleType> types;
};

struct CachedText {
    SDL_Texture* texture;
    std::string text;
    int width, height;

    CachedText() : texture(nullptr), text(""), width(0), height(0) {}

    ~CachedText() {
        if (texture) SDL_DestroyTexture(texture);
    }
};



// Helper to create a scene object from a sprite file at a position
std::shared_ptr<SceneObject> createSpriteObject(const std::string& spritePath, float x, float y, SDL_Renderer* renderer) {
    auto sprite = std::make_shared<Sprite>();
    if (!sprite->load(spritePath, renderer)) {
        std::cerr << "Failed to load sprite: " << spritePath << std::endl;
        return nullptr;
    }
    auto obj = std::make_shared<SceneObject>();
    obj->setSprite(sprite);
    obj->setPosition(x, y);
    return obj;
}

void drawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dstRect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void drawCachedText(SDL_Renderer* renderer, TTF_Font* font, CachedText& cache,
                    const std::string& text, int x, int y, SDL_Color color) {
    if (cache.text != text || cache.texture == nullptr) {
        if (cache.texture) {
            SDL_DestroyTexture(cache.texture);
            cache.texture = nullptr;
        }

        SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
        if (!surface) return;

        cache.texture = SDL_CreateTextureFromSurface(renderer, surface);
        cache.width = surface->w;
        cache.height = surface->h;
        cache.text = text;

        SDL_FreeSurface(surface);
    }

    if (cache.texture) {
        SDL_Rect dstRect = {x, y, cache.width, cache.height};
        SDL_RenderCopy(renderer, cache.texture, nullptr, &dstRect);
    }
}

void drawDropdown(SDL_Renderer* renderer, TTF_Font* font, const UIDropdown& dropdown) {
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(renderer, &dropdown.rect);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &dropdown.rect);

    SDL_Color textColor = {255, 255, 255, 255};
    drawText(renderer, font, dropdown.options[dropdown.selectedIndex],
             dropdown.rect.x + 5, dropdown.rect.y + 5, textColor);

    drawText(renderer, font, "v",
             dropdown.rect.x + dropdown.rect.w - 20, dropdown.rect.y + 5, textColor);

    if (dropdown.isOpen) {
        for (size_t i = 0; i < dropdown.options.size(); ++i) {
            SDL_Rect optionRect = {
                dropdown.rect.x,
                dropdown.rect.y + dropdown.rect.h * (int)(i + 1),
                dropdown.rect.w,
                dropdown.rect.h
            };

            if (i == dropdown.selectedIndex) {
                SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
            }
            SDL_RenderFillRect(renderer, &optionRect);

            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderDrawRect(renderer, &optionRect);

            drawText(renderer, font, dropdown.options[i],
                     optionRect.x + 5, optionRect.y + 5, textColor);
        }
    }
}

bool handleDropdownClick(UIDropdown& dropdown, int mouseX, int mouseY) {
    if (mouseX >= dropdown.rect.x && mouseX < dropdown.rect.x + dropdown.rect.w &&
        mouseY >= dropdown.rect.y && mouseY < dropdown.rect.y + dropdown.rect.h) {
        dropdown.isOpen = !dropdown.isOpen;
        return true;
    }

    if (dropdown.isOpen) {
        for (size_t i = 0; i < dropdown.options.size(); ++i) {
            SDL_Rect optionRect = {
                dropdown.rect.x,
                dropdown.rect.y + dropdown.rect.h * (int)(i + 1),
                dropdown.rect.w,
                dropdown.rect.h
            };

            if (mouseX >= optionRect.x && mouseX < optionRect.x + optionRect.w &&
                mouseY >= optionRect.y && mouseY < optionRect.y + optionRect.h) {
                dropdown.selectedIndex = i;
                dropdown.isOpen = false;
                return true;
            }
        }
    }

    return false;
}

int main(int argc, char* argv[]) {
    Config config;
    SandSimulator sandSimulator(config);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << "\n";
        return 1;
    }

    if (TTF_Init() < 0) {
        std::cerr << "SDL_ttf initialization failed: " << TTF_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_DisplayMode displayMode;
    if (SDL_GetDesktopDisplayMode(0, &displayMode) != 0) {
        std::cerr << "Failed to get display mode: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Particle Simulator - World Mode",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        displayMode.w,
        displayMode.h,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL
    );

    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_GetCurrentContext();
    if (!glContext) {
        glContext = SDL_GL_CreateContext(window);
        if (!glContext) {
            std::cerr << "OpenGL context creation failed: " << SDL_GetError() << "\n";
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            return 1;
        }
    }

    if (SDL_GL_MakeCurrent(window, glContext) < 0) {
        std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << "\n";
    }

    // OpenGL extensions not needed for SDL2 software rendering

    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
    if (!font) {
        std::cerr << "Font loading failed: " << TTF_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font* smallFont = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 10);
    if (!smallFont) {
        std::cerr << "Small font loading failed: " << TTF_GetError() << "\n";
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Create World
    World world(config);

    // Get actual window size (respects OS rotation for portrait monitors)
    int actualWindowW, actualWindowH;
    SDL_GetWindowSize(window, &actualWindowW, &actualWindowH);

    // Uniform scaling: 300 pixels tall, width based on aspect ratio
    int viewportHeight = 300;
    float uniformScale = (float)actualWindowH / (float)viewportHeight;
    int viewportWidth = (int)(actualWindowW / uniformScale);
    world.setViewportSize(viewportWidth, viewportHeight);

    // Uniform scale for both axes (square pixels)
    float scaleX = uniformScale;
    float scaleY = uniformScale;

    // DEBUG: Print viewport calculation
    std::cout << "ActualWindow: " << actualWindowW << "x" << actualWindowH
              << " Viewport: " << viewportWidth << "x" << viewportHeight
              << " Scale: " << uniformScale << std::endl;

    // Initialize background layers (mountains)
    ZLayers zLayers;
    zLayers.init(renderer, viewportWidth, viewportHeight, World::WORLD_WIDTH);

    // Initialize main sprite sheet for enemies, etc.
    MainSprite mainSprite;
    if (!mainSprite.load("scenes/mainSprite.png", renderer)) {
        std::cerr << "Failed to load main sprite sheet!" << std::endl;
    }
    // Define little_purple_jumper: standing (0,0 to 4,7) and jumping (6,0 to 10,7)
    mainSprite.defineSprite("little_purple_jumper", {
        {0, 0, 5, 8},   // Frame 0: standing
        {6, 0, 5, 8}    // Frame 1: jumping
    });

    // Define inventory slot sprite (16,0 to 24,8) - 8x8 pixels
    mainSprite.defineSprite("inventory_slot", {
        {16, 0, 8, 8}
    });

    // Vector to hold enemies
    std::vector<LittlePurpleJumper> purpleJumpers;

    // Set scene image - will lazy load chunks as they come into view
    world.setSceneImage("scenes/level1.png");

    // Create player sprite
    auto playerSprite = std::make_shared<Sprite>();
    if (!playerSprite->load("scenes/sprite1.png", renderer)) {
        std::cerr << "Failed to load player sprite!" << std::endl;
    }

    // Create player as scene object
    auto player = std::make_shared<SceneObject>();
    player->setSprite(playerSprite);
    player->setCollider(1, -10, 9, 19);  // Keep box collider for particle blocking
    player->setCapsuleCollider(5.0f, 14.0f, 5.5f, 5.0f);  // Capsule for terrain collision
    player->setBlocksParticles(true);

    // Position player at (100, 100) from bottom-left origin
    // World Y increases downward, so bottom of world = WORLD_HEIGHT
    float playerStartX = 47.0f;
    float playerStartY = 26065.0f;
    player->setPosition(playerStartX, playerStartY);

    world.addSceneObject(player);

    // Create orb1 collectible
    std::vector<std::shared_ptr<Collectible>> collectibles;
    auto orb1 = std::make_shared<Collectible>();
    if (orb1->create("scenes/orb1.png", 581.0f, 25750.0f, renderer)) {
        world.addSceneObject(orb1->getSceneObject());
        collectibles.push_back(orb1);
    }

    // Create bullet doubler collectible (100 right, 50 up from player start)
    // Uses sprite from mainSprite at (16,0) - 8x8 pixels
    std::shared_ptr<Collectible> bulletDoublerPickup = nullptr;
    auto doubler = std::make_shared<Collectible>();
    // For now use orb1 as placeholder until we create a sprite-based collectible
    if (doubler->create("scenes/orb1.png", playerStartX + 100.0f, playerStartY - 50.0f, renderer)) {
        world.addSceneObject(doubler->getSceneObject());
        bulletDoublerPickup = doubler;
    }

    // Create gun1 collectible
    std::vector<std::shared_ptr<Gun>> guns;
    std::shared_ptr<Gun> equippedGun = nullptr;
    auto gun1 = std::make_shared<Gun>();
    if (gun1->create("scenes/gun1.png", 106.0f, 26062.0f, renderer)) {
        world.addSceneObject(gun1->getSceneObject());
        guns.push_back(gun1);

        // Configure wand stats
        gun1->getStats().name = "Starter Wand";
        gun1->getStats().maxMana = 150;
        gun1->getStats().currentMana = 150;
        gun1->getStats().manaRechargeRate = 40.0f;
        gun1->getStats().castDelay = 0.1f;
        gun1->getStats().rechargeTime = 0.2f;

        // Add ammunition types to the gun
        auto sparkBolt = std::make_shared<SparkBolt>();
        auto bouncingBolt = std::make_shared<BouncingBolt>();
        auto fireBolt = std::make_shared<FireBolt>();
        auto magicMissile = std::make_shared<MagicMissile>();

        // Add modifiers to some spells
        bouncingBolt->addModifier(std::make_shared<DamageUpModifier>(1.5f));
        magicMissile->addModifier(std::make_shared<PiercingModifier>(2));

        gun1->addAmmunition(sparkBolt);
        gun1->addAmmunition(bouncingBolt);
        gun1->addAmmunition(fireBolt);
        gun1->addAmmunition(magicMissile);
    }

    // Position camera to center on player (with bounds clamping)
    Camera& cam = world.getCamera();
    // Center on player - offset Y down so player appears lower on screen initially
    cam.centerOn(playerStartX + 2.0f, playerStartY - 50.0f,
                 (float)World::WORLD_WIDTH, (float)World::WORLD_HEIGHT);

    // Camera will smoothly follow player with deadzone
    // This keeps the player centered even at world edges

    SDL_Texture* viewportTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,  // Match Uint32 pixel format
        SDL_TEXTUREACCESS_STREAMING,
        viewportWidth,
        viewportHeight
    );
    if (!viewportTexture) {
        std::cerr << "Viewport texture creation failed: " << SDL_GetError() << "\n";
        TTF_CloseFont(smallFont);
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    // Enable alpha blending so ZLayers show through transparent pixels
    SDL_SetTextureBlendMode(viewportTexture, SDL_BLENDMODE_BLEND);

    std::vector<Uint32> pixels(viewportWidth * viewportHeight);

    // Setup material dropdown
    UIDropdown dropdown;
    dropdown.rect = {10, 10, 120, 30};
    dropdown.isOpen = false;
    dropdown.selectedIndex = 0;
    dropdown.options = {"Sand", "Water", "Rock", "Wood", "Lava", "Steam", "Fire", "Obsidian", "Ice", "Glass", "Erase"};
    dropdown.types = {ParticleType::SAND, ParticleType::WATER, ParticleType::ROCK, ParticleType::WOOD, ParticleType::LAVA, ParticleType::STEAM, ParticleType::FIRE, ParticleType::OBSIDIAN, ParticleType::ICE, ParticleType::GLASS, ParticleType::EMPTY};

    // Setup volume dropdown
    UIDropdown volumeDropdown;
    volumeDropdown.rect = {140, 10, 80, 30};
    volumeDropdown.isOpen = false;
    volumeDropdown.selectedIndex = 3;  // Default to 1000 particles
    volumeDropdown.options = {"1", "10", "100", "1000"};
    volumeDropdown.types = {};
    std::vector<int> volumeValues = {1, 10, 100, 1000};

    // Setup framerate dropdown
    UIDropdown fpsDropdown;
    fpsDropdown.rect = {230, 10, 70, 30};
    fpsDropdown.isOpen = false;
    fpsDropdown.selectedIndex = 5;  // Default to 100 FPS
    fpsDropdown.options = {"1", "10", "15", "30", "60", "100"};
    fpsDropdown.types = {};
    std::vector<int> fpsValues = {1, 10, 15, 30, 60, 100};

    bool running = true;

    Uint32 fpsTimer = SDL_GetTicks();
    int frameCount = 0;
    float currentFPS = 0.0f;

    Uint32 lastFrameTime = SDL_GetTicks();

    CachedText fpsCache;
    CachedText posCache;
    CachedText sandCountCache, waterCountCache, rockCountCache, lavaCountCache;
    CachedText steamCountCache, fireCountCache, obsidianCountCache, iceCountCache, glassCountCache;

    int targetFPS = fpsValues[fpsDropdown.selectedIndex];
    int frameDelay = 1000 / targetFPS;

    float last_sim_time = 0;
    float last_render_time = 0;

    // Movement state
    bool moveLeft = false, moveRight = false;
    bool shiftHeld = false;
    bool thrustHeld = false;  // Spacebar for jetpack thrust
    bool eKeyPressed = false;  // For collectible interaction
    bool inventoryOpen = false;  // Toggle with 'I' key

    // Inventory items (true = collected)
    bool hasBulletDoubler = false;

    // Player physics
    float playerVelX = 0.0f;
    float playerVelY = 0.0f;
    const float GRAVITY = 400.0f;        // Pixels per second squared
    const float THRUST_POWER = 600.0f;   // Upward acceleration when thrusting
    const float MOVE_ACCEL = 600.0f;     // Horizontal acceleration
    const float MAX_FALL_SPEED = 300.0f; // Terminal velocity
    const float AIR_FRICTION = 0.95f;    // Horizontal slowdown in air
    const float GROUND_FRICTION = 0.9f;  // Horizontal slowdown on ground

    while (running) {
        Uint32 frameStart = SDL_GetTicks();
        float deltaTime = (frameStart - lastFrameTime) / 1000.0f;
        lastFrameTime = frameStart;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_F11: {
                        Uint32 flags = SDL_GetWindowFlags(window);
                        if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                            SDL_SetWindowFullscreen(window, 0);
                        } else {
                            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        }
                        break;
                    }
                    case SDLK_a:
                        moveLeft = true;
                        break;
                    case SDLK_d:
                        moveRight = true;
                        break;
                    case SDLK_LSHIFT:
                    case SDLK_RSHIFT:
                        shiftHeld = true;
                        break;
                    case SDLK_SPACE:
                        thrustHeld = true;
                        break;
                    case SDLK_e:
                        eKeyPressed = true;
                        break;
                    case SDLK_i:
                        inventoryOpen = !inventoryOpen;
                        break;
                }
            }
            else if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_a:
                        moveLeft = false;
                        break;
                    case SDLK_d:
                        moveRight = false;
                        break;
                    case SDLK_LSHIFT:
                    case SDLK_RSHIFT:
                        shiftHeld = false;
                        break;
                    case SDLK_SPACE:
                        thrustHeld = false;
                        break;
                    case SDLK_e:
                        eKeyPressed = false;
                        break;
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Check dropdowns first
                    int prevFpsIdx = fpsDropdown.selectedIndex;
                    if (handleDropdownClick(fpsDropdown, event.button.x, event.button.y)) {
                        dropdown.isOpen = false;
                        volumeDropdown.isOpen = false;
                        if (fpsDropdown.selectedIndex != prevFpsIdx) {
                            targetFPS = fpsValues[fpsDropdown.selectedIndex];
                            frameDelay = 1000 / targetFPS;
                        }
                    } else if (handleDropdownClick(volumeDropdown, event.button.x, event.button.y)) {
                        dropdown.isOpen = false;
                        fpsDropdown.isOpen = false;
                    } else if (handleDropdownClick(dropdown, event.button.x, event.button.y)) {
                        volumeDropdown.isOpen = false;
                        fpsDropdown.isOpen = false;
                    } else if (equippedGun && equippedGun->isEquipped()) {
                        Uint32 currentTime = SDL_GetTicks();
                        if (equippedGun->canFire(currentTime)) {
                            equippedGun->fire(world);

                        }
                    }
                }
            }
        }

                        // ==================== Player Physics ====================

                        const auto& cap = player->getCapsule();

                        float collisionY;

                

                        // 1. Ground Check

                        bool onGround = world.checkCapsuleCollision(player->getX() + cap.offsetX, player->getY() + cap.offsetY + 1, cap.radius, cap.height, collisionY);

                

                        // 2. Apply vertical forces

                        if (onGround) {

                            if (!thrustHeld) {

                                playerVelY = 0;

                            }

                        } else {

                            playerVelY += GRAVITY * deltaTime;

                        }

                        

                        if (thrustHeld) {

                            playerVelY -= THRUST_POWER * deltaTime;

                        }

                

                        // 3. Horizontal forces and friction

                        float dx = 0;

                        if (moveLeft) dx = -1;

                        if (moveRight) dx = 1;

                        if (dx != 0) {

                            playerVelX += dx * MOVE_ACCEL * deltaTime;

                        }

                        

                        if (onGround) {

                            playerVelX *= GROUND_FRICTION;

                        } else {

                            playerVelX *= AIR_FRICTION;

                        }

                

                        // 4. Clamp velocities

                        playerVelY = std::min(playerVelY, MAX_FALL_SPEED);

                        playerVelY = std::max(playerVelY, -MAX_FALL_SPEED);

                        float maxHorizSpeed = shiftHeld ? 200.0f : 100.0f;

                        playerVelX = std::max(-maxHorizSpeed, std::min(playerVelX, maxHorizSpeed));

                

                        // 5. Calculate new positions

                        float newX = player->getX() + playerVelX * deltaTime;

                        float newY = player->getY() + playerVelY * deltaTime;

                        

                        // 6. Resolve Collisions

                        // Y-axis first

                        float capsuleCenterX = player->getX() + cap.offsetX;

                        float capsuleCenterY = player->getY() + cap.offsetY;

                        if (world.checkCapsuleCollision(capsuleCenterX, capsuleCenterY, cap.radius, cap.height, collisionY)) {

                            if (playerVelY > 0) {

                                newY = collisionY - (cap.offsetY + cap.height + cap.radius);

                            }

                            playerVelY = 0;

                        }

                

                        // X-axis
                        capsuleCenterX = newX + cap.offsetX;
                        capsuleCenterY = player->getY() + cap.offsetY; // Use current Y for this check
                        if (world.checkCapsuleCollision(capsuleCenterX, capsuleCenterY, cap.radius, cap.height, collisionY)) {
                            bool steppedUp = false;
                            if (onGround) {
                                for (int step = 1; step <= 2; ++step) {
                                    float dummy;
                                    if (!world.checkCapsuleCollision(capsuleCenterX, capsuleCenterY - step, cap.radius, cap.height, dummy)) {
                                        newY = player->getY() - step;
                                        steppedUp = true;
                                        break;
                                    }
                                }
                            }
                            if (!steppedUp) {
                                newX = player->getX();
                                playerVelX = 0;
                            }
                        }

                

                        // 7. Set final position

                        player->setPosition(newX, newY);

                        

                        // 8. Clamp player to world bounds

                        float px = player->getX();

                        float py = player->getY();

                        if (px < 0) { px = 0; playerVelX = 0; }

                        if (px > World::WORLD_WIDTH - 11) { px = World::WORLD_WIDTH - 11; playerVelX = 0; }

                        if (py < 0) { py = 0; playerVelY = 0; }

                        if (py > World::WORLD_HEIGHT - 24) { py = World::WORLD_HEIGHT - 24; playerVelY = 0; }

                        player->setPosition(px, py);
        // Camera follows player with deadzone, smooth follow, and look-ahead
        float camDirX = (playerVelX > 10) ? 1 : (playerVelX < -10 ? -1 : 0);
        float camDirY = (playerVelY > 10) ? 1 : (playerVelY < -10 ? -1 : 0);
        cam.update(player->getX(), player->getY(), 4.0f, 11.0f,  // player pos and size
                   camDirX, camDirY,  // movement direction for look-ahead
                   (float)World::WORLD_WIDTH, (float)World::WORLD_HEIGHT, deltaTime);

        // Update and check collectibles
        float playerW = (float)playerSprite->getWidth();
        float playerH = (float)playerSprite->getHeight();
        for (auto& collectible : collectibles) {
            if (collectible->isActive()) {
                collectible->checkCollection(player->getX(), player->getY(), playerW, playerH, eKeyPressed);
                collectible->update(deltaTime);
            }
        }

        // Check for bullet doubler collection
        if (bulletDoublerPickup && !hasBulletDoubler && bulletDoublerPickup->isActive()) {
            if (bulletDoublerPickup->checkCollection(player->getX(), player->getY(), playerW, playerH, eKeyPressed)) {
                hasBulletDoubler = true;
                std::cout << "Bullet Doubler acquired!" << std::endl;

                // Double the projectile count for all ammunition on equipped gun
                if (equippedGun) {
                    for (int i = 0; i < equippedGun->getAmmunitionCount(); ++i) {
                        auto ammo = equippedGun->getAmmunition(i);
                        if (ammo) {
                            ammo->projectileCount *= 2;
                        }
                    }
                }
            }
            bulletDoublerPickup->update(deltaTime);
        }

        // Check for gun collection
        for (auto& gun : guns) {
            if (!gun->isEquipped() && !gun->isCollected()) {
                if (gun->checkCollection(player->getX(), player->getY(), playerW, playerH, eKeyPressed)) {
                    equippedGun = gun;
                }
            }
        }

        // Update equipped gun position and rotation toward cursor
        if (equippedGun && equippedGun->isEquipped()) {
            int curMouseX, curMouseY;
            SDL_GetMouseState(&curMouseX, &curMouseY);
            float cursorWorldX = cam.x + (curMouseX / scaleX);
            float cursorWorldY = cam.y + (curMouseY / scaleY);
            float playerCenterX = player->getX() + playerW / 2.0f;
            float playerCenterY = player->getY() + playerH / 2.0f;
            equippedGun->updateEquipped(playerCenterX, playerCenterY, cursorWorldX, cursorWorldY);
            equippedGun->update(deltaTime);  // Update mana recharge
        }

        // Spawn enemies from detected spawn points
        for (auto& spawnPoint : world.getEnemySpawnPoints()) {
            if (!spawnPoint.spawned && spawnPoint.type == SpawnMarkerType::LITTLE_PURPLE_JUMPER) {
                LittlePurpleJumper jumper;
                // Position at spawn point, offset up by sprite height so feet are at marker
                jumper.init((float)spawnPoint.worldX, (float)spawnPoint.worldY - 7.0f, &mainSprite, &world);
                purpleJumpers.push_back(jumper);
                spawnPoint.spawned = true;
            }
        }

        // Update enemies
        float playerCenterXForEnemy = player->getX() + playerW / 2.0f;
        float playerCenterYForEnemy = player->getY() + playerH / 2.0f;
        for (auto& jumper : purpleJumpers) {
            jumper.update(deltaTime, playerCenterXForEnemy, playerCenterYForEnemy);
        }

        // Update gun ammunition (bullets)
        if (equippedGun && equippedGun->isEquipped()) {
            equippedGun->updateAmmunition(deltaTime, world, purpleJumpers);
        }

                // Ensure OpenGL context is current

                if (SDL_GL_MakeCurrent(window, glContext) < 0) {

                    static bool logged = false;

                    if (!logged) {

                        std::cerr << "SDL_GL_MakeCurrent failed in loop: " << SDL_GetError() << "\n";

                        logged = true;

                    }

                }

        

                Uint32 rendering_start = SDL_GetTicks();

        

                // Clear screen to black

                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

                SDL_RenderClear(renderer);

                // Render background layers (mountains) before particles
                zLayers.render(renderer, cam.x, cam.y, viewportWidth, viewportHeight, scaleX, scaleY);

                // --- Optimized particle rendering ---
                // Visible region is exactly viewportWidth x viewportHeight (512x512)
                int visWorldX_start = (int)cam.x;
                int visWorldY_start = (int)cam.y;
                int visWorldX_end = visWorldX_start + viewportWidth;
                int visWorldY_end = visWorldY_start + viewportHeight;

        

                int startChunkX, startChunkY, endChunkX, endChunkY;

                World::worldToChunk(visWorldX_start, visWorldY_start, startChunkX, startChunkY);

                World::worldToChunk(visWorldX_end - 1, visWorldY_end - 1, endChunkX, endChunkY);

        

                std::fill(pixels.begin(), pixels.end(), 0x00000000);  // Transparent so zlayers show through

        

                for (int cy = startChunkY; cy <= endChunkY; ++cy) {

                    for (int cx = startChunkX; cx <= endChunkX; ++cx) {

                        const WorldChunk* chunk = world.getChunk(cx, cy);

                        if (!chunk) continue;

        

                        const auto& colors = chunk->getColorGrid();

                        const auto& particles = chunk->getParticleGrid();

                        int chunkWorldX = chunk->getWorldX();

                        int chunkWorldY = chunk->getWorldY();

        

                        int loop_x_start = std::max(visWorldX_start, chunkWorldX);

                        int loop_y_start = std::max(visWorldY_start, chunkWorldY);

                        int loop_x_end = std::min(visWorldX_end, chunkWorldX + WorldChunk::CHUNK_SIZE);

                        int loop_y_end = std::min(visWorldY_end, chunkWorldY + WorldChunk::CHUNK_SIZE);

        

                        for (int worldY = loop_y_start; worldY < loop_y_end; ++worldY) {

                            int screenY = worldY - visWorldY_start;

                            int localY = worldY - chunkWorldY;

                            int pixel_idx_base = screenY * viewportWidth;

                            int chunk_idx_base = localY * WorldChunk::CHUNK_SIZE;

        

                            for (int worldX = loop_x_start; worldX < loop_x_end; ++worldX) {

                                int screenX = worldX - visWorldX_start;

                                int localX = worldX - chunkWorldX;

        

                                int chunk_idx = chunk_idx_base + localX;

                                if (particles[chunk_idx] != ParticleType::EMPTY) {

                                    const auto& color = colors[chunk_idx];

                                    pixels[pixel_idx_base + screenX] = 0xFF000000 | (color.r << 16) | (color.g << 8) | color.b;

                                }

                            }

                        }

                    }

                }

        

        

                // Render gun ammunition (bullets) to pixel buffer BEFORE updating texture
                if (equippedGun && equippedGun->isEquipped()) {
                    equippedGun->renderAmmunition(renderer, pixels, viewportWidth, viewportHeight, cam.x, cam.y, scaleX, scaleY);
                }

                // Update texture
                SDL_UpdateTexture(viewportTexture, nullptr, pixels.data(), viewportWidth * sizeof(Uint32));

        

                // Render texture to screen with explicit scaling
                SDL_Rect destRect = {0, 0, actualWindowW, actualWindowH};
                SDL_RenderCopy(renderer, viewportTexture, nullptr, &destRect);

        

                // Draw fire effects

                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);

                for (int y = 0; y < viewportHeight; ++y) {

                    for (int x = 0; x < viewportWidth; ++x) {

                        int worldX = (int)cam.x + x;

                        int worldY = (int)cam.y + y;

        

                        if (world.getParticle(worldX, worldY) == ParticleType::FIRE) {

                            ParticleColor fireColor = world.getColor(worldX, worldY);

        

                            for (int dy = -3; dy <= 0; ++dy) {

                                int screenY = y + dy;

                                if (screenY >= 0 && screenY < viewportHeight) {

                                    float intensity = 1.0f - (float)(-dy) / 4.0f;

                                    int alpha = (int)(intensity * 120);

        

                                    SDL_Rect flameRect = {x, screenY, 1, 1};

                                    SDL_SetRenderDrawColor(renderer,

                                        (int)(fireColor.r * intensity),

                                        (int)(fireColor.g * intensity * 0.7f),

                                        0, alpha);

                                    SDL_RenderFillRect(renderer, &flameRect);

                                }

                            }

                        }

                    }

                }

                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        
                // Render scene objects (sprites)
                for (const auto& obj : world.getSceneObjects()) {
                    if (!obj || !obj->isVisible()) continue;
        
                    Sprite* spr = obj->getSprite();
                    if (!spr || !spr->isLoaded()) continue;
        
                    int screenX = (int)((obj->getX() - cam.x) * scaleX);
                    int screenY = (int)((obj->getY() - cam.y) * scaleY);
                    int screenW = (int)(spr->getWidth() * scaleX);
                    int screenH = (int)(spr->getHeight() * scaleY);
        
                    SDL_Texture* tex = spr->getTexture();
                    SDL_Rect dstRect = {screenX, screenY, screenW, screenH};
                    SDL_RenderCopy(renderer, tex, nullptr, &dstRect);
                    obj->renderHealthBar(renderer, cam.x, cam.y, scaleX, scaleY);
                }

                // Render collectible explosion particles
                for (auto& collectible : collectibles) {
                    collectible->render(renderer, cam.x, cam.y, scaleX, scaleY);
                }
                if (bulletDoublerPickup) {
                    bulletDoublerPickup->render(renderer, cam.x, cam.y, scaleX, scaleY);
                }

        

                // Render equipped gun

                if (equippedGun && equippedGun->isEquipped()) {

                    equippedGun->renderEquipped(renderer, cam.x, cam.y, scaleX, scaleY);

                }

        
                // Render enemies
                for (auto& jumper : purpleJumpers) {
                    jumper.render(renderer, cam.x, cam.y, scaleX, scaleY);
                    jumper.renderHealthBar(renderer, cam.x, cam.y, scaleX, scaleY);
                }

                        // Debug chunk outlines disabled
                // Draw outlines for awake particle chunks
                /*
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 100); // Red, semi-transparent
                int p_visWorldX_start, p_visWorldY_start, p_visWorldX_end, p_visWorldY_end;
                world.getVisibleRegion(p_visWorldX_start, p_visWorldY_start, p_visWorldX_end, p_visWorldY_end);
                int startPCX, startPCY, endPCX, endPCY;
                World::worldToParticleChunk(p_visWorldX_start, p_visWorldY_start, startPCX, startPCY);
                World::worldToParticleChunk(p_visWorldX_end - 1, p_visWorldY_end - 1, endPCX, endPCY);
                const auto& particleChunks = world.getParticleChunks();
                for (int pcY = startPCY; pcY <= endPCY; ++pcY) {
                    for (int pcX = startPCX; pcX <= endPCX; ++pcX) {
                        if(pcX < 0 || pcX >= World::P_CHUNKS_X || pcY < 0 || pcY >= World::P_CHUNKS_Y) continue;
                        int pcIndex = pcY * World::P_CHUNKS_X + pcX;
                        if (particleChunks[pcIndex].isAwake) {
                            SDL_Rect rect;
                            rect.x = (int)((pcX * World::PARTICLE_CHUNK_WIDTH - cam.x) * scaleX);
                            rect.y = (int)((pcY * World::PARTICLE_CHUNK_HEIGHT - cam.y) * scaleY);
                            rect.w = (int)(World::PARTICLE_CHUNK_WIDTH * scaleX);
                            rect.h = (int)(World::PARTICLE_CHUNK_HEIGHT * scaleY);
                            SDL_RenderDrawRect(renderer, &rect);
                        }
                    }
                }
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                */

                        // Draw player colliders (disabled)

                        // const auto& p_box = player->getCollider();

        

                // Draw UI

                drawDropdown(renderer, font, dropdown);

                drawDropdown(renderer, font, volumeDropdown);

                drawDropdown(renderer, font, fpsDropdown);


        

                // Update FPS counter

                frameCount++;

                Uint32 currentTime = SDL_GetTicks();

                if (currentTime - fpsTimer >= 1000) {

                    currentFPS = frameCount / ((currentTime - fpsTimer) / 1000.0f);

                    frameCount = 0;

                    fpsTimer = currentTime;

                }

        

                // Draw mana bar (top right)
                if (equippedGun && equippedGun->isEquipped()) {
                    int manaBarWidth = 120;
                    int manaBarHeight = 12;
                    int manaBarX = actualWindowW - manaBarWidth - 10;
                    int manaBarY = 10;

                    // Background
                    SDL_SetRenderDrawColor(renderer, 20, 20, 40, 200);
                    SDL_Rect manaBg = {manaBarX - 2, manaBarY - 2, manaBarWidth + 4, manaBarHeight + 4};
                    SDL_RenderFillRect(renderer, &manaBg);

                    // Empty bar
                    SDL_SetRenderDrawColor(renderer, 30, 30, 60, 255);
                    SDL_Rect manaEmpty = {manaBarX, manaBarY, manaBarWidth, manaBarHeight};
                    SDL_RenderFillRect(renderer, &manaEmpty);

                    // Filled bar (blue gradient effect)
                    float manaPercent = equippedGun->getManaPercent();
                    int filledWidth = (int)(manaBarWidth * manaPercent);
                    if (filledWidth > 0) {
                        SDL_SetRenderDrawColor(renderer, 50, 100, 255, 255);
                        SDL_Rect manaFilled = {manaBarX, manaBarY, filledWidth, manaBarHeight};
                        SDL_RenderFillRect(renderer, &manaFilled);

                        // Highlight on top
                        SDL_SetRenderDrawColor(renderer, 100, 150, 255, 255);
                        SDL_Rect manaHighlight = {manaBarX, manaBarY, filledWidth, 3};
                        SDL_RenderFillRect(renderer, &manaHighlight);
                    }

                    // Border
                    SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255);
                    SDL_RenderDrawRect(renderer, &manaEmpty);

                    // Mana text
                    SDL_Color manaTextColor = {150, 180, 255, 255};
                    std::string manaText = std::to_string(equippedGun->getMana()) + "/" + std::to_string(equippedGun->getMaxMana());
                    drawText(renderer, smallFont, manaText, manaBarX + manaBarWidth / 2 - 15, manaBarY - 1, manaTextColor);
                }

                // Draw stats

                SDL_Color whiteColor = {255, 255, 255, 255};

                std::string fpsText = "FPS: " + std::to_string((int)currentFPS);

                drawCachedText(renderer, smallFont, fpsCache, fpsText, 5, actualWindowH - 15, whiteColor);

        

                float playerCenterX = player->getX() + playerSprite->getWidth() / 2.0f;

                float playerCenterY = player->getY() + playerSprite->getHeight() / 2.0f;

                std::string posText = "Pos: " + std::to_string((int)playerCenterX) + ", " + std::to_string((int)playerCenterY);

                drawCachedText(renderer, smallFont, posCache, posText, 5, actualWindowH - 30, whiteColor);

        

                // Draw particle counts

                int yOffset = actualWindowH - (12 * 9) - 5; // 9 lines of text, 12 pixels per line, 5 pixels padding

                int xPos = actualWindowW - 100;

        

                int sandCount = world.getParticleCount(ParticleType::SAND);

                int waterCount = world.getParticleCount(ParticleType::WATER);

                int rockCount = world.getParticleCount(ParticleType::ROCK);

                int lavaCount = world.getParticleCount(ParticleType::LAVA);

                int steamCount = world.getParticleCount(ParticleType::STEAM);

                int fireCount = world.getParticleCount(ParticleType::FIRE);

                int obsidianCount = world.getParticleCount(ParticleType::OBSIDIAN);

                int iceCount = world.getParticleCount(ParticleType::ICE);

                int glassCount = world.getParticleCount(ParticleType::GLASS);

        

                SDL_Color sandColor = {255, 200, 100, 255};

                drawCachedText(renderer, smallFont, sandCountCache, "Sand: " + std::to_string(sandCount), xPos, yOffset, sandColor);

                yOffset += 12;

        

                SDL_Color waterColor = {50, 100, 255, 255};

                drawCachedText(renderer, smallFont, waterCountCache, "Water: " + std::to_string(waterCount), xPos, yOffset, waterColor);

                yOffset += 12;

        

                SDL_Color rockColor = {128, 128, 128, 255};

                drawCachedText(renderer, smallFont, rockCountCache, "Rock: " + std::to_string(rockCount), xPos, yOffset, rockColor);

                yOffset += 12;

        

                SDL_Color lavaColor = {255, 100, 0, 255};

                drawCachedText(renderer, smallFont, lavaCountCache, "Lava: " + std::to_string(lavaCount), xPos, yOffset, lavaColor);

                yOffset += 12;

        

                SDL_Color steamColor = {200, 200, 200, 255};

                drawCachedText(renderer, smallFont, steamCountCache, "Steam: " + std::to_string(steamCount), xPos, yOffset, steamColor);

                yOffset += 12;

        

                SDL_Color fireColor = {255, 100, 0, 255};

                drawCachedText(renderer, smallFont, fireCountCache, "Fire: " + std::to_string(fireCount), xPos, yOffset, fireColor);

                yOffset += 12;

        

                SDL_Color obsidianColor = {100, 90, 110, 255};

                drawCachedText(renderer, smallFont, obsidianCountCache, "Obsidian: " + std::to_string(obsidianCount), xPos, yOffset, obsidianColor);

                yOffset += 12;

        

                SDL_Color iceColor = {200, 230, 255, 255};

                drawCachedText(renderer, smallFont, iceCountCache, "Ice: " + std::to_string(iceCount), xPos, yOffset, iceColor);

                yOffset += 12;

        

                SDL_Color glassColor = {100, 180, 180, 255};

                drawCachedText(renderer, smallFont, glassCountCache, "Glass: " + std::to_string(glassCount), xPos, yOffset, glassColor);

                yOffset += 12;

        

                // Draw controls hint

                SDL_Color hintColor = {150, 150, 150, 255};

                drawText(renderer, smallFont, "WASD to move, Shift for fast", 5, actualWindowH - 45, hintColor);

                // Draw inventory if open
                if (inventoryOpen) {
                    // Calculate inventory position (centered on screen)
                    int slotSize = 32;  // Scaled size of each slot
                    int slotPadding = 4;
                    int slotsPerRow = 5;
                    int numSlots = 10;
                    int numRows = (numSlots + slotsPerRow - 1) / slotsPerRow;

                    int invWidth = slotsPerRow * (slotSize + slotPadding) + slotPadding;
                    int invHeight = numRows * (slotSize + slotPadding) + slotPadding;
                    int invX = (actualWindowW - invWidth) / 2;
                    int invY = (actualWindowH - invHeight) / 2;

                    // Draw background box
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 220);
                    SDL_Rect invBg = {invX - 10, invY - 30, invWidth + 20, invHeight + 40};
                    SDL_RenderFillRect(renderer, &invBg);

                    // Draw border
                    SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
                    SDL_RenderDrawRect(renderer, &invBg);

                    // Draw title
                    SDL_Color titleColor = {255, 255, 255, 255};
                    drawText(renderer, font, "Inventory", invX, invY - 25, titleColor);

                    // Draw 10 inventory slots
                    SDL_Texture* slotTex = mainSprite.getTexture();
                    for (int i = 0; i < numSlots; ++i) {
                        int row = i / slotsPerRow;
                        int col = i % slotsPerRow;
                        int slotX = invX + slotPadding + col * (slotSize + slotPadding);
                        int slotY = invY + slotPadding + row * (slotSize + slotPadding);

                        // Draw empty slot background
                        SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
                        SDL_Rect slotBg = {slotX, slotY, slotSize, slotSize};
                        SDL_RenderFillRect(renderer, &slotBg);
                        SDL_SetRenderDrawColor(renderer, 70, 70, 90, 255);
                        SDL_RenderDrawRect(renderer, &slotBg);

                        // Draw item in slot 0 if bullet doubler is collected
                        if (i == 0 && hasBulletDoubler && slotTex) {
                            SDL_SetTextureBlendMode(slotTex, SDL_BLENDMODE_BLEND);
                            SDL_Rect srcRect = {16, 0, 8, 8};  // Bullet doubler sprite
                            SDL_Rect dstRect = {slotX + 4, slotY + 4, slotSize - 8, slotSize - 8};
                            SDL_RenderCopy(renderer, slotTex, &srcRect, &dstRect);
                        }
                    }

                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                }

                SDL_RenderPresent(renderer);

        

                last_render_time = SDL_GetTicks() - rendering_start;

        

        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameDelay > (int)frameTime) {
            SDL_Delay(frameDelay - frameTime);
        }
    }

    TTF_CloseFont(smallFont);
    TTF_CloseFont(font);
    SDL_DestroyTexture(viewportTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}

