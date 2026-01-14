#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <string>
#include "Config.h"
#include "SandSimulator.h"

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
    // Only recreate texture if text changed
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
    // Draw main button
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_RenderFillRect(renderer, &dropdown.rect);
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawRect(renderer, &dropdown.rect);

    // Draw selected text
    SDL_Color textColor = {255, 255, 255, 255};
    drawText(renderer, font, dropdown.options[dropdown.selectedIndex],
             dropdown.rect.x + 5, dropdown.rect.y + 5, textColor);

    // Draw dropdown arrow
    drawText(renderer, font, "v",
             dropdown.rect.x + dropdown.rect.w - 20, dropdown.rect.y + 5, textColor);

    // Draw options if open
    if (dropdown.isOpen) {
        for (size_t i = 0; i < dropdown.options.size(); ++i) {
            SDL_Rect optionRect = {
                dropdown.rect.x,
                dropdown.rect.y + dropdown.rect.h * (int)(i + 1),
                dropdown.rect.w,
                dropdown.rect.h
            };

            // Highlight selected option
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
    // Check if clicking the main button
    if (mouseX >= dropdown.rect.x && mouseX < dropdown.rect.x + dropdown.rect.w &&
        mouseY >= dropdown.rect.y && mouseY < dropdown.rect.y + dropdown.rect.h) {
        dropdown.isOpen = !dropdown.isOpen;
        return true;
    }

    // Check if clicking an option (when open)
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

    // Get desktop display mode for fullscreen
    SDL_DisplayMode displayMode;
    if (SDL_GetDesktopDisplayMode(0, &displayMode) != 0) {
        std::cerr << "Failed to get display mode: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Keep the game grid size from rules.txt (e.g., 512x512)
    // This is the VIRTUAL game space - don't change it
    // windowWidth and windowHeight in config define the virtual grid dimensions

    SDL_Window* window = SDL_CreateWindow(
        "Particle Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        displayMode.w,
        displayMode.h,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load fonts (using system default)
    TTF_Font* font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
    if (!font) {
        std::cerr << "Font loading failed: " << TTF_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load small font for stats
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

    SandSimulator simulator(config);

    // Create texture for fast rendering (this is the VIRTUAL grid size from rules.txt)
    int gridWidth = simulator.getWidth();
    int gridHeight = simulator.getHeight();

    // Stretch the virtual grid to fill entire screen (no aspect ratio preservation)
    SDL_Rect renderRect;
    renderRect.x = 0;
    renderRect.y = 0;
    renderRect.w = displayMode.w;  // Full screen width
    renderRect.h = displayMode.h;  // Full screen height

    // Calculate scale factors for mouse input (can be different for X and Y)
    float scaleX = (float)displayMode.w / (float)gridWidth;
    float scaleY = (float)displayMode.h / (float)gridHeight;
    SDL_Texture* gridTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB888,
        SDL_TEXTUREACCESS_STREAMING,
        gridWidth,
        gridHeight
    );
    if (!gridTexture) {
        std::cerr << "Texture creation failed: " << SDL_GetError() << "\n";
        TTF_CloseFont(smallFont);
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Allocate pixel buffer for texture updates
    std::vector<Uint32> pixels(gridWidth * gridHeight);

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
    volumeDropdown.selectedIndex = 0;
    volumeDropdown.options = {"1", "10", "100", "1000"};
    volumeDropdown.types = {}; // Not used for volume, just store as int
    std::vector<int> volumeValues = {1, 10, 100, 1000};

    bool running = true;

    // FPS tracking
    Uint32 fpsTimer = SDL_GetTicks();
    int frameCount = 0;
    float currentFPS = 0.0f;

    // Cached text for performance
    CachedText fpsCache;
    CachedText sandCountCache, waterCountCache, rockCountCache, lavaCountCache;
    CachedText steamCountCache, fireCountCache, obsidianCountCache, iceCountCache, glassCountCache;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    // Single ESC press exits the application
                    running = false;
                }
                else if (event.key.keysym.sym == SDLK_F11) {
                    // F11 toggles fullscreen
                    Uint32 flags = SDL_GetWindowFlags(window);
                    if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                        SDL_SetWindowFullscreen(window, 0);
                    } else {
                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Try volume dropdown first, then material dropdown
                    if (handleDropdownClick(volumeDropdown, event.button.x, event.button.y)) {
                        // Close material dropdown if volume was clicked
                        dropdown.isOpen = false;
                    } else if (handleDropdownClick(dropdown, event.button.x, event.button.y)) {
                        // Close volume dropdown if material was clicked
                        volumeDropdown.isOpen = false;
                    }
                }
            }
        }

        // Handle mouse dragging for particle spawning
        int mouseX, mouseY;
        Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
        if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) {
            // Check if mouse is not over any dropdown
            bool overDropdown = false;

            // Check material dropdown
            if (mouseX >= dropdown.rect.x && mouseX < dropdown.rect.x + dropdown.rect.w) {
                int dropdownHeight = dropdown.rect.h;
                if (dropdown.isOpen) {
                    dropdownHeight *= (dropdown.options.size() + 1);
                }
                if (mouseY >= dropdown.rect.y && mouseY < dropdown.rect.y + dropdownHeight) {
                    overDropdown = true;
                }
            }

            // Check volume dropdown
            if (mouseX >= volumeDropdown.rect.x && mouseX < volumeDropdown.rect.x + volumeDropdown.rect.w) {
                int dropdownHeight = volumeDropdown.rect.h;
                if (volumeDropdown.isOpen) {
                    dropdownHeight *= (volumeDropdown.options.size() + 1);
                }
                if (mouseY >= volumeDropdown.rect.y && mouseY < volumeDropdown.rect.y + dropdownHeight) {
                    overDropdown = true;
                }
            }

            if (!overDropdown) {
                // Convert screen coordinates to grid coordinates
                // Grid now fills entire screen, so no offset needed

                // Check if mouse is within the screen
                if (mouseX >= 0 && mouseX < displayMode.w &&
                    mouseY >= 0 && mouseY < displayMode.h) {

                    int gridX = (int)(mouseX / scaleX);
                    int gridY = (int)(mouseY / scaleY);

                    // Get selected volume
                    int volume = volumeValues[volumeDropdown.selectedIndex];

                    // Spawn particles at mouse position
                    ParticleType selectedType = dropdown.types[dropdown.selectedIndex];

                    for (int i = 0; i < volume; ++i) {
                        // Add some randomness to position for volume > 1
                        int offsetX = (volume > 1) ? (std::rand() % 5 - 2) : 0;
                        int offsetY = (volume > 1) ? (std::rand() % 5 - 2) : 0;
                        int spawnX = gridX + offsetX;
                        int spawnY = gridY + offsetY;

                        if (selectedType == ParticleType::EMPTY) {
                            // Erase mode - set to empty
                            if (spawnX >= 0 && spawnX < simulator.getWidth() &&
                                spawnY >= 0 && spawnY < simulator.getHeight()) {
                                simulator.setParticleType(spawnX, spawnY, ParticleType::EMPTY);
                            }
                        } else if (selectedType == ParticleType::ROCK) {
                            // Spawn rock cluster
                            simulator.spawnRockCluster(spawnX, spawnY);
                        } else if (selectedType == ParticleType::WOOD) {
                            // Spawn wood cluster
                            simulator.spawnWoodCluster(spawnX, spawnY);
                        } else {
                            simulator.spawnParticleAt(spawnX, spawnY, selectedType);
                        }
                    }
                }
            }
        }

        simulator.update();

        // Clear screen to black
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Fill pixel buffer with particle data
        for (int y = 0; y < gridHeight; ++y) {
            for (int x = 0; x < gridWidth; ++x) {
                int idx = y * gridWidth + x;
                if (simulator.isOccupied(x, y)) {
                    ParticleColor color = simulator.getColor(x, y);
                    pixels[idx] = (color.r << 16) | (color.g << 8) | color.b;
                } else {
                    pixels[idx] = 0x000000; // Black for empty
                }
            }
        }

        // Update texture with pixel data
        SDL_UpdateTexture(gridTexture, nullptr, pixels.data(), gridWidth * sizeof(Uint32));

        // Render texture to calculated position (handles letterboxing)
        SDL_RenderCopy(renderer, gridTexture, nullptr, &renderRect);

        // Draw flame effects on top of fire particles
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        for (int y = 0; y < gridHeight; ++y) {
            for (int x = 0; x < gridWidth; ++x) {
                if (simulator.getParticleType(x, y) == ParticleType::FIRE) {
                    // Draw flame pixels above and around this fire particle
                    ParticleColor fireColor = simulator.getColor(x, y);

                    // Draw flames above (with decreasing intensity)
                    for (int dy = -3; dy <= 0; ++dy) {
                        int flameY = y + dy;
                        if (flameY >= 0 && flameY < gridHeight) {
                            // Intensity decreases as we go up
                            float intensity = 1.0f - (float)(-dy) / 4.0f;
                            int alpha = (int)(intensity * 120);

                            // Map grid coordinates to screen coordinates
                            int screenX = renderRect.x + (int)(x * scaleX);
                            int screenY = renderRect.y + (int)(flameY * scaleY);
                            int screenW = (int)(scaleX) + 1;
                            int screenH = (int)(scaleY) + 1;

                            SDL_Rect flameRect = {screenX, screenY, screenW, screenH};
                            SDL_SetRenderDrawColor(renderer,
                                (int)(fireColor.r * intensity),
                                (int)(fireColor.g * intensity * 0.7f),
                                0, alpha);
                            SDL_RenderFillRect(renderer, &flameRect);
                        }
                    }

                    // Draw side flames occasionally for flicker effect
                    if ((x + y + frameCount) % 3 == 0) {
                        for (int dx = -1; dx <= 1; dx += 2) {
                            int flameX = x + dx;
                            if (flameX >= 0 && flameX < gridWidth) {
                                int screenX = renderRect.x + (int)(flameX * scaleX);
                                int screenY = renderRect.y + (int)(y * scaleY);
                                int screenW = (int)(scaleX) + 1;
                                int screenH = (int)(scaleY) + 1;

                                SDL_Rect flameRect = {screenX, screenY, screenW, screenH};
                                SDL_SetRenderDrawColor(renderer,
                                    fireColor.r / 2,
                                    fireColor.g / 3,
                                    0, 80);
                                SDL_RenderFillRect(renderer, &flameRect);
                            }
                        }
                    }
                }
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // Draw debug visualization - red boxes around active chunks
        int chunkWidth = simulator.getChunkWidth();
        int chunkHeight = simulator.getChunkHeight();
        int chunksX = simulator.getChunksX();
        int chunksY = simulator.getChunksY();

        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Red for active chunks
        for (int cy = 0; cy < chunksY; ++cy) {
            for (int cx = 0; cx < chunksX; ++cx) {
                // Only draw boxes around awake (not sleeping) chunks
                if (simulator.isChunkActiveForDebug(cx, cy) && !simulator.isChunkSleepingForDebug(cx, cy)) {
                    // Convert chunk coordinates to screen coordinates
                    int chunkGridX = cx * chunkWidth;
                    int chunkGridY = cy * chunkHeight;
                    int chunkGridW = chunkWidth;
                    int chunkGridH = chunkHeight;

                    // Map to screen space
                    int screenX = (int)(chunkGridX * scaleX);
                    int screenY = (int)(chunkGridY * scaleY);
                    int screenW = (int)(chunkGridW * scaleX);
                    int screenH = (int)(chunkGridH * scaleY);

                    SDL_Rect chunkRect = {screenX, screenY, screenW, screenH};
                    SDL_RenderDrawRect(renderer, &chunkRect);
                }
            }
        }

        // Draw UI on top
        drawDropdown(renderer, font, dropdown);
        drawDropdown(renderer, font, volumeDropdown);

        // Update FPS counter
        frameCount++;
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - fpsTimer >= 1000) { // Update FPS every second
            currentFPS = frameCount / ((currentTime - fpsTimer) / 1000.0f);
            frameCount = 0;
            fpsTimer = currentTime;
        }

        // Draw FPS (bottom left) - use screen dimensions, not game grid
        SDL_Color whiteColor = {255, 255, 255, 255};
        std::string fpsText = "FPS: " + std::to_string((int)currentFPS);
        drawCachedText(renderer, smallFont, fpsCache, fpsText, 5, displayMode.h - 15, whiteColor);

        // Count active and sleeping chunks for debug display
        int activeChunks = 0;
        int sleepingChunks = 0;
        for (int cy = 0; cy < chunksY; ++cy) {
            for (int cx = 0; cx < chunksX; ++cx) {
                if (simulator.isChunkActiveForDebug(cx, cy)) {
                    if (simulator.isChunkSleepingForDebug(cx, cy)) {
                        sleepingChunks++;
                    } else {
                        activeChunks++;
                    }
                }
            }
        }

        // Draw chunk stats (bottom left, below FPS)
        int totalChunks = chunksX * chunksY;
        std::string chunkText = "Active: " + std::to_string(activeChunks) + " / Sleep: " + std::to_string(sleepingChunks) + " / Total: " + std::to_string(totalChunks);
        drawText(renderer, smallFont, chunkText, 5, displayMode.h - 30, whiteColor);

        // Draw particle counts (top right) - use screen dimensions
        int yOffset = 5;
        int xPos = displayMode.w - 100;

        // Count and display each particle type
        int sandCount = simulator.getParticleCount(ParticleType::SAND);
        int waterCount = simulator.getParticleCount(ParticleType::WATER);
        int rockCount = simulator.getParticleCount(ParticleType::ROCK);
        int lavaCount = simulator.getParticleCount(ParticleType::LAVA);
        int steamCount = simulator.getParticleCount(ParticleType::STEAM);
        int fireCount = simulator.getParticleCount(ParticleType::FIRE);
        int obsidianCount = simulator.getParticleCount(ParticleType::OBSIDIAN);
        int iceCount = simulator.getParticleCount(ParticleType::ICE);
        int glassCount = simulator.getParticleCount(ParticleType::GLASS);

        if (sandCount > 0) {
            SDL_Color sandColor = {255, 200, 100, 255};
            drawCachedText(renderer, smallFont, sandCountCache, "Sand: " + std::to_string(sandCount), xPos, yOffset, sandColor);
            yOffset += 12;
        }
        if (waterCount > 0) {
            SDL_Color waterColor = {50, 100, 255, 255};
            drawCachedText(renderer, smallFont, waterCountCache, "Water: " + std::to_string(waterCount), xPos, yOffset, waterColor);
            yOffset += 12;
        }
        if (rockCount > 0) {
            SDL_Color rockColor = {128, 128, 128, 255};
            drawCachedText(renderer, smallFont, rockCountCache, "Rock: " + std::to_string(rockCount), xPos, yOffset, rockColor);
            yOffset += 12;
        }
        if (lavaCount > 0) {
            SDL_Color lavaColor = {255, 100, 0, 255};
            drawCachedText(renderer, smallFont, lavaCountCache, "Lava: " + std::to_string(lavaCount), xPos, yOffset, lavaColor);
            yOffset += 12;
        }
        if (steamCount > 0) {
            SDL_Color steamColor = {200, 200, 200, 255};
            drawCachedText(renderer, smallFont, steamCountCache, "Steam: " + std::to_string(steamCount), xPos, yOffset, steamColor);
            yOffset += 12;
        }
        if (fireCount > 0) {
            SDL_Color fireColor = {255, 100, 0, 255};
            drawCachedText(renderer, smallFont, fireCountCache, "Fire: " + std::to_string(fireCount), xPos, yOffset, fireColor);
            yOffset += 12;
        }
        if (obsidianCount > 0) {
            SDL_Color obsidianColor = {30, 20, 40, 255};
            drawCachedText(renderer, smallFont, obsidianCountCache, "Obsidian: " + std::to_string(obsidianCount), xPos, yOffset, obsidianColor);
            yOffset += 12;
        }
        if (iceCount > 0) {
            SDL_Color iceColor = {200, 230, 255, 255};
            drawCachedText(renderer, smallFont, iceCountCache, "Ice: " + std::to_string(iceCount), xPos, yOffset, iceColor);
            yOffset += 12;
        }
        if (glassCount > 0) {
            SDL_Color glassColor = {100, 180, 180, 255};
            drawCachedText(renderer, smallFont, glassCountCache, "Glass: " + std::to_string(glassCount), xPos, yOffset, glassColor);
            yOffset += 12;
        }

        SDL_RenderPresent(renderer);
        // SDL_Delay(16); // Removed frame cap to show true performance
    }

    TTF_CloseFont(smallFont);
    TTF_CloseFont(font);
    SDL_DestroyTexture(gridTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
