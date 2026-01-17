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

// Bullet projectile for player shooting
struct Bullet {
    float x, y;           // World position
    float vx, vy;         // Velocity (normalized direction * speed)
    bool active;          // Is this bullet still alive?
    static constexpr float SPEED = 500.0f;       // Pixels per second
    static constexpr int EXPLOSION_RADIUS = 18;  // Explosion radius on impact
    static constexpr float EXPLOSION_FORCE = 14.0f; // Force applied to particles

    Bullet() : x(0), y(0), vx(0), vy(0), active(false) {}

    Bullet(float startX, float startY, float targetX, float targetY)
        : x(startX), y(startY), active(true) {
        float dx = targetX - startX;
        float dy = targetY - startY;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.1f) {
            vx = (dx / len) * SPEED;
            vy = (dy / len) * SPEED;
        } else {
            vx = SPEED;
            vy = 0;
        }
    }

    // Update bullet position, return true if hit something
    bool update(World& world, float deltaTime) {
        if (!active) return false;

        float newX = x + vx * deltaTime;
        float newY = y + vy * deltaTime;

        // Check for collision along the path (simple raycast)
        int steps = std::max(1, (int)(SPEED * deltaTime / 2.0f)); // Check every 2 pixels
        float stepX = (newX - x) / steps;
        float stepY = (newY - y) / steps;

        for (int i = 0; i <= steps; i++) {
            int checkX = (int)(x + stepX * i);
            int checkY = (int)(y + stepY * i);

            // Check world bounds
            if (!world.inWorldBounds(checkX, checkY)) {
                active = false;
                return false;
            }

            // Check for particle hit
            if (world.isOccupied(checkX, checkY)) {
                // Explode at this position
                world.explodeAt(checkX, checkY, EXPLOSION_RADIUS, EXPLOSION_FORCE);
                active = false;
                return true;
            }
        }

        x = newX;
        y = newY;
        return false;
    }
};

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
    if (!config.loadFromFile("rules.txt")) {
        std::cout << "Using default configuration\n";
    }

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

    // Viewport is the game's virtual size (from rules.txt, e.g. 400x400)
    // This gets scaled up to fill the screen
    int viewportWidth = config.windowWidth;
    int viewportHeight = config.windowHeight;
    world.setViewportSize(viewportWidth, viewportHeight);

    // Set scene image - will lazy load chunks as they come into view
    world.setSceneImage("../scenes/level1.png");

    // Create player sprite
    auto playerSprite = std::make_shared<Sprite>();
    if (!playerSprite->load("../scenes/sprite1.png", renderer)) {
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
    float playerStartX = 20.0f;
    float playerStartY = World::WORLD_HEIGHT - 20.0f - 11.0f;  // 20 from bottom, minus sprite height
    player->setPosition(playerStartX, playerStartY);

    world.addSceneObject(player);

    // Position camera to center on player (with bounds clamping)
    Camera& cam = world.getCamera();
    cam.centerOn(playerStartX + 5.5f, playerStartY + 12.0f,
                 (float)World::WORLD_WIDTH, (float)World::WORLD_HEIGHT);

    // Camera will smoothly follow player with deadzone
    // This keeps the player centered even at world edges

    SDL_Texture* viewportTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB888,
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

    std::vector<Uint32> pixels(viewportWidth * viewportHeight);

    // Scale factors for mouse input (screen -> viewport)
    float scaleX = (float)displayMode.w / (float)viewportWidth;
    float scaleY = (float)displayMode.h / (float)viewportHeight;

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

    // Movement state
    bool moveLeft = false, moveRight = false;
    bool shiftHeld = false;
    bool thrustHeld = false;  // Spacebar for jetpack thrust

    // Player physics
    float playerVelX = 0.0f;
    float playerVelY = 0.0f;
    const float GRAVITY = 400.0f;        // Pixels per second squared
    const float THRUST_POWER = 600.0f;   // Upward acceleration when thrusting
    const float MOVE_ACCEL = 600.0f;     // Horizontal acceleration
    const float MAX_FALL_SPEED = 300.0f; // Terminal velocity
    const float AIR_FRICTION = 0.95f;    // Horizontal slowdown in air
    const float GROUND_FRICTION = 0.9f;  // Horizontal slowdown on ground

    // Bullets for shooting
    std::vector<Bullet> bullets;
    bullets.reserve(50); // Pre-allocate for performance

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
                    } else {
                        // Fire bullet toward cursor (left click when not on dropdown)
                        float worldMouseX = cam.x + (event.button.x / scaleX);
                        float worldMouseY = cam.y + (event.button.y / scaleY);
                        float bulletStartX = player->getX() + 2.0f;
                        float bulletStartY = player->getY() + 5.5f;
                        bullets.emplace_back(bulletStartX, bulletStartY, worldMouseX, worldMouseY);
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

        // Update bullets
        for (auto& bullet : bullets) {
            bullet.update(world, deltaTime);
        }
        // Remove inactive bullets
        bullets.erase(
            std::remove_if(bullets.begin(), bullets.end(),
                [](const Bullet& b) { return !b.active; }),
            bullets.end());

        // Handle mouse dragging for particle spawning (right click)
        int mouseX, mouseY;
        Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
        if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
            bool overDropdown = false;

            if (mouseX >= dropdown.rect.x && mouseX < dropdown.rect.x + dropdown.rect.w) {
                int dropdownHeight = dropdown.rect.h;
                if (dropdown.isOpen) {
                    dropdownHeight *= (dropdown.options.size() + 1);
                }
                if (mouseY >= dropdown.rect.y && mouseY < dropdown.rect.y + dropdownHeight) {
                    overDropdown = true;
                }
            }

            if (mouseX >= volumeDropdown.rect.x && mouseX < volumeDropdown.rect.x + volumeDropdown.rect.w) {
                int dropdownHeight = volumeDropdown.rect.h;
                if (volumeDropdown.isOpen) {
                    dropdownHeight *= (volumeDropdown.options.size() + 1);
                }
                if (mouseY >= volumeDropdown.rect.y && mouseY < volumeDropdown.rect.y + dropdownHeight) {
                    overDropdown = true;
                }
            }

            if (mouseX >= fpsDropdown.rect.x && mouseX < fpsDropdown.rect.x + fpsDropdown.rect.w) {
                int dropdownHeight = fpsDropdown.rect.h;
                if (fpsDropdown.isOpen) {
                    dropdownHeight *= (fpsDropdown.options.size() + 1);
                }
                if (mouseY >= fpsDropdown.rect.y && mouseY < fpsDropdown.rect.y + dropdownHeight) {
                    overDropdown = true;
                }
            }

            if (!overDropdown) {
                // Convert screen coordinates to viewport coordinates, then to world coordinates
                int viewX = (int)(mouseX / scaleX);
                int viewY = (int)(mouseY / scaleY);
                int worldX = (int)cam.x + viewX;
                int worldY = (int)cam.y + viewY;

                int volume = volumeValues[volumeDropdown.selectedIndex];
                ParticleType selectedType = dropdown.types[dropdown.selectedIndex];

                // Calculate radius for a filled circle with ~volume pixels
                // Area = pi*r^2, so r = sqrt(volume/pi)
                int radius = std::max(1, (int)std::sqrt((float)volume / 3.14159f));

                // Spawn particles in a filled circle
                for (int dy = -radius; dy <= radius; dy++) {
                    for (int dx = -radius; dx <= radius; dx++) {
                        if (dx * dx + dy * dy <= radius * radius) {
                            int spawnX = worldX + dx;
                            int spawnY = worldY + dy;

                            if (selectedType == ParticleType::EMPTY) {
                                world.setParticle(spawnX, spawnY, ParticleType::EMPTY);
                            } else {
                                world.spawnParticleAt(spawnX, spawnY, selectedType);
                            }
                        }
                    }
                }
            }
        }

        // Update world simulation
        for (int i = 0; i < config.fallSpeed; ++i) {
            world.update(deltaTime / config.fallSpeed);
        }

        // Ensure OpenGL context is current
        if (SDL_GL_MakeCurrent(window, glContext) < 0) {
            static bool logged = false;
            if (!logged) {
                std::cerr << "SDL_GL_MakeCurrent failed in loop: " << SDL_GetError() << "\n";
                logged = true;
            }
        }

        // Clear screen to black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Fill pixel buffer with visible particles
        int visStartX, visStartY, visEndX, visEndY;
        world.getVisibleRegion(visStartX, visStartY, visEndX, visEndY);

        for (int y = 0; y < viewportHeight; ++y) {
            for (int x = 0; x < viewportWidth; ++x) {
                int worldX = (int)cam.x + x;
                int worldY = (int)cam.y + y;
                int idx = y * viewportWidth + x;

                if (world.isOccupied(worldX, worldY)) {
                    ParticleColor color = world.getColor(worldX, worldY);
                    pixels[idx] = (color.r << 16) | (color.g << 8) | color.b;
                } else {
                    if (worldX >= 0 && worldX < World::WORLD_WIDTH &&
                        worldY >= 0 && worldY < World::WORLD_HEIGHT) {
                        pixels[idx] = 0x000010;  // Very dark blue
                    } else {
                        pixels[idx] = 0x200000;  // Dark red for outside world
                    }
                }
            }
        }

        // Draw bullets as white crosses
        for (const auto& bullet : bullets) {
            if (!bullet.active) continue;
            int screenX = (int)(bullet.x - cam.x);
            int screenY = (int)(bullet.y - cam.y);

            // Draw a small white cross (5x5)
            const Uint32 white = 0xFFFFFF;
            for (int i = -2; i <= 2; i++) {
                // Horizontal line
                int px = screenX + i;
                int py = screenY;
                if (px >= 0 && px < viewportWidth && py >= 0 && py < viewportHeight) {
                    pixels[py * viewportWidth + px] = white;
                }
                // Vertical line
                px = screenX;
                py = screenY + i;
                if (px >= 0 && px < viewportWidth && py >= 0 && py < viewportHeight) {
                    pixels[py * viewportWidth + px] = white;
                }
            }
        }

        // Update texture
        SDL_UpdateTexture(viewportTexture, nullptr, pixels.data(), viewportWidth * sizeof(Uint32));

        // Render texture to screen
        SDL_RenderCopy(renderer, viewportTexture, nullptr, nullptr);

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

            SDL_Rect dstRect = {screenX, screenY, screenW, screenH};
            SDL_RenderCopy(renderer, spr->getTexture(), nullptr, &dstRect);
        }

        // Draw player colliders
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        
        // Draw box collider
        const auto& p_box = player->getCollider();
        SDL_Rect box_rect = {
            (int)((player->getX() + p_box.x - cam.x) * scaleX),
            (int)((player->getY() + p_box.y - cam.y) * scaleY),
            (int)(p_box.width * scaleX),
            (int)(p_box.height * scaleY)
        };
        SDL_RenderDrawRect(renderer, &box_rect);

        // Draw capsule collider
        const auto& p_cap = player->getCapsule();
        float p_x = player->getX();
        float p_y = player->getY();

        float capsule_cx_world = p_x + p_cap.offsetX;
        float top_circle_cy_world = p_y + p_cap.offsetY;
        float bottom_circle_cy_world = top_circle_cy_world + p_cap.height;

        // Draw top semi-circle
        for (int angle = 0; angle <= 180; angle += 15) {
            float rad1 = angle * M_PI / 180.0f;
            float rad2 = (angle + 15) * M_PI / 180.0f;
            int x1 = (int)(((capsule_cx_world + std::cos(rad1) * p_cap.radius) - cam.x) * scaleX);
            int y1 = (int)(((top_circle_cy_world - std::sin(rad1) * p_cap.radius) - cam.y) * scaleY);
            int x2 = (int)(((capsule_cx_world + std::cos(rad2) * p_cap.radius) - cam.x) * scaleX);
            int y2 = (int)(((top_circle_cy_world - std::sin(rad2) * p_cap.radius) - cam.y) * scaleY);
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }

        // Draw bottom semi-circle
        for (int angle = 180; angle <= 360; angle += 15) {
            float rad1 = angle * M_PI / 180.0f;
            float rad2 = (angle + 15) * M_PI / 180.0f;
            int x1 = (int)(((capsule_cx_world + std::cos(rad1) * p_cap.radius) - cam.x) * scaleX);
            int y1 = (int)(((bottom_circle_cy_world - std::sin(rad1) * p_cap.radius) - cam.y) * scaleY);
            int x2 = (int)(((capsule_cx_world + std::cos(rad2) * p_cap.radius) - cam.x) * scaleX);
            int y2 = (int)(((bottom_circle_cy_world - std::sin(rad2) * p_cap.radius) - cam.y) * scaleY);
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
        
        // Draw vertical lines
        int x1_left = (int)(((capsule_cx_world - p_cap.radius) - cam.x) * scaleX);
        int y1_left = (int)((top_circle_cy_world - cam.y) * scaleY);
        int x2_left = (int)(((capsule_cx_world - p_cap.radius) - cam.x) * scaleX);
        int y2_left = (int)((bottom_circle_cy_world - cam.y) * scaleY);
        SDL_RenderDrawLine(renderer, x1_left, y1_left, x2_left, y2_left);

        int x1_right = (int)(((capsule_cx_world + p_cap.radius) - cam.x) * scaleX);
        int y1_right = (int)((top_circle_cy_world - cam.y) * scaleY);
        int x2_right = (int)(((capsule_cx_world + p_cap.radius) - cam.x) * scaleX);
        int y2_right = (int)((bottom_circle_cy_world - cam.y) * scaleY);
        SDL_RenderDrawLine(renderer, x1_right, y1_right, x2_right, y2_right);

        // Draw UI
        drawDropdown(renderer, font, dropdown);
        drawDropdown(renderer, font, volumeDropdown);
        drawDropdown(renderer, font, fpsDropdown);

        // Draw ground colliders using edge detection
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        for (int y = visStartY; y < visEndY; ++y) {
            for (int x = visStartX; x < visEndX; ++x) {
                bool is_solid = world.isSolidParticle(world.getParticle(x, y));
                bool up_is_solid = world.isSolidParticle(world.getParticle(x, y - 1));
                bool left_is_solid = world.isSolidParticle(world.getParticle(x - 1, y));

                if (is_solid != up_is_solid) {
                    SDL_RenderDrawLine(renderer, (int)((x - cam.x) * scaleX), (int)((y - cam.y) * scaleY), (int)((x + 1 - cam.x) * scaleX), (int)((y - cam.y) * scaleY));
                }
                if (is_solid != left_is_solid) {
                    SDL_RenderDrawLine(renderer, (int)((x - cam.x) * scaleX), (int)((y - cam.y) * scaleY), (int)((x - cam.x) * scaleX), (int)((y + 1 - cam.y) * scaleY));
                }
            }
        }

        // Update FPS counter
        frameCount++;
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - fpsTimer >= 1000) {
            currentFPS = frameCount / ((currentTime - fpsTimer) / 1000.0f);
            frameCount = 0;
            fpsTimer = currentTime;
        }

        // Draw stats
        SDL_Color whiteColor = {255, 255, 255, 255};
        std::string fpsText = "FPS: " + std::to_string((int)currentFPS);
        drawCachedText(renderer, smallFont, fpsCache, fpsText, 5, displayMode.h - 15, whiteColor);

        std::string posText = "Pos: " + std::to_string((int)cam.x) + ", " + std::to_string((int)cam.y);
        drawCachedText(renderer, smallFont, posCache, posText, 5, displayMode.h - 30, whiteColor);

        // Draw particle counts
        int yOffset = 5;
        int xPos = displayMode.w - 100;

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
        drawText(renderer, smallFont, "WASD to move, Shift for fast", 5, displayMode.h - 45, hintColor);

        SDL_RenderPresent(renderer);

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
