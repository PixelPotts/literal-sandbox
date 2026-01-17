#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>

// A sprite frame extracted from a sprite sheet
struct SpriteFrame {
    int x, y;           // Position in sprite sheet
    int width, height;  // Size of frame
};

// Sprite class for loading and managing sprite images
class Sprite {
public:
    Sprite();
    ~Sprite();

    // Load sprite from file
    bool load(const std::string& filepath, SDL_Renderer* renderer);

    // Load a single frame from the sprite sheet
    // frameX, frameY are in frame units (not pixels)
    // frameWidth, frameHeight are the size of each frame in pixels
    bool loadFrame(const std::string& filepath, SDL_Renderer* renderer,
                   int frameX, int frameY, int frameWidth, int frameHeight);

    // Load a region from sprite sheet using pixel coordinates
    // startX, startY are top-left corner in pixels
    // regionWidth, regionHeight are the size in pixels
    bool loadRegion(const std::string& filepath, SDL_Renderer* renderer,
                    int startX, int startY, int regionWidth, int regionHeight);

    // Get the SDL texture
    SDL_Texture* getTexture() const { return texture; }

    // Get outline texture (white silhouette for glow effect)
    SDL_Texture* getOutlineTexture() const { return outlineTexture; }

    // Generate outline texture (call after load)
    void generateOutline(SDL_Renderer* renderer, int radius = 2);

    // Get dimensions
    int getWidth() const { return width; }
    int getHeight() const { return height; }

    // Get raw pixel data (for collision detection)
    const std::vector<unsigned char>& getPixels() const { return pixels; }

    // Check if a pixel is solid (non-transparent)
    bool isPixelSolid(int x, int y) const;

    // Get pixel color at position
    void getPixelColor(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b, unsigned char& a) const;

    bool isLoaded() const { return texture != nullptr; }

private:
    SDL_Texture* texture;
    SDL_Texture* outlineTexture;
    int width, height;
    std::vector<unsigned char> pixels;  // RGBA pixel data for collision
    int channels;
};
