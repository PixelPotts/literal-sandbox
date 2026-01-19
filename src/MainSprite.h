#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

// A frame region within the main sprite sheet
struct SpriteRegion {
    int x, y;           // Top-left position in sprite sheet (pixels)
    int width, height;  // Size of region (pixels)
};

// A named sprite definition with one or more animation frames
struct SpriteDefinition {
    std::string name;
    std::vector<SpriteRegion> frames;
};

// MainSprite: A shared sprite sheet for enemies, bullets, effects, etc.
// Loads once, provides frame regions by name.
class MainSprite {
public:
    MainSprite();
    ~MainSprite();

    // Load the main sprite sheet
    bool load(const std::string& filepath, SDL_Renderer* renderer);

    // Define a named sprite with frames (call after load)
    void defineSprite(const std::string& name, const std::vector<SpriteRegion>& frames);

    // Get a sprite definition by name
    const SpriteDefinition* getSprite(const std::string& name) const;

    // Get the underlying texture
    SDL_Texture* getTexture() const { return texture; }

    // Get sheet dimensions
    int getSheetWidth() const { return sheetWidth; }
    int getSheetHeight() const { return sheetHeight; }

    // Check if loaded
    bool isLoaded() const { return texture != nullptr; }

    // Render a specific frame of a named sprite
    void renderFrame(SDL_Renderer* renderer, const std::string& spriteName, int frameIndex,
                     float worldX, float worldY, float cameraX, float cameraY,
                     float scaleX, float scaleY, bool flipHorizontal = false);

private:
    SDL_Texture* texture;
    int sheetWidth;
    int sheetHeight;

    std::unordered_map<std::string, SpriteDefinition> sprites;
};
