#include "Sprite.h"
// Allow large images
#define STBI_MAX_DIMENSIONS 33554432
#include "stb_image.h"
#include <iostream>

Sprite::Sprite()
    : texture(nullptr)
    , width(0)
    , height(0)
    , channels(0)
{
}

Sprite::~Sprite() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

bool Sprite::load(const std::string& filepath, SDL_Renderer* renderer) {
    // Load image with stb_image
    int imgWidth, imgHeight, imgChannels;
    unsigned char* data = stbi_load(filepath.c_str(), &imgWidth, &imgHeight, &imgChannels, 4);  // Force RGBA

    if (!data) {
        std::cerr << "Failed to load sprite: " << filepath << std::endl;
        std::cerr << "stbi error: " << stbi_failure_reason() << std::endl;
        return false;
    }

    width = imgWidth;
    height = imgHeight;
    channels = 4;  // We forced RGBA

    // Store pixel data for collision detection
    pixels.assign(data, data + (width * height * channels));

    // Create SDL texture
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        data, width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32
    );

    if (!surface) {
        std::cerr << "Failed to create surface: " << SDL_GetError() << std::endl;
        stbi_image_free(data);
        return false;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    stbi_image_free(data);

    if (!texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }

    // Enable alpha blending
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    std::cout << "Loaded sprite: " << filepath << " (" << width << "x" << height << ")" << std::endl;
    return true;
}

bool Sprite::loadFrame(const std::string& filepath, SDL_Renderer* renderer,
                       int frameX, int frameY, int frameWidth, int frameHeight) {
    // Load full image first
    int imgWidth, imgHeight, imgChannels;
    unsigned char* data = stbi_load(filepath.c_str(), &imgWidth, &imgHeight, &imgChannels, 4);

    if (!data) {
        std::cerr << "Failed to load sprite sheet: " << filepath << std::endl;
        std::cerr << "stbi error: " << stbi_failure_reason() << std::endl;
        return false;
    }

    // Calculate frame position in pixels
    int startX = frameX * frameWidth;
    int startY = frameY * frameHeight;

    // Validate bounds
    if (startX + frameWidth > imgWidth || startY + frameHeight > imgHeight) {
        std::cerr << "Frame out of bounds in sprite sheet" << std::endl;
        stbi_image_free(data);
        return false;
    }

    width = frameWidth;
    height = frameHeight;
    channels = 4;

    // Extract frame pixels
    pixels.resize(width * height * channels);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcIdx = ((startY + y) * imgWidth + (startX + x)) * 4;
            int dstIdx = (y * width + x) * 4;
            pixels[dstIdx + 0] = data[srcIdx + 0];  // R
            pixels[dstIdx + 1] = data[srcIdx + 1];  // G
            pixels[dstIdx + 2] = data[srcIdx + 2];  // B
            pixels[dstIdx + 3] = data[srcIdx + 3];  // A
        }
    }

    stbi_image_free(data);

    // Create SDL texture from extracted frame
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels.data(), width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32
    );

    if (!surface) {
        std::cerr << "Failed to create surface: " << SDL_GetError() << std::endl;
        return false;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    std::cout << "Loaded sprite frame: " << filepath << " frame(" << frameX << "," << frameY
              << ") size " << width << "x" << height << std::endl;
    return true;
}

bool Sprite::loadRegion(const std::string& filepath, SDL_Renderer* renderer,
                        int startX, int startY, int regionWidth, int regionHeight) {
    // Load full image first
    int imgWidth, imgHeight, imgChannels;
    unsigned char* data = stbi_load(filepath.c_str(), &imgWidth, &imgHeight, &imgChannels, 4);

    if (!data) {
        std::cerr << "Failed to load sprite sheet: " << filepath << std::endl;
        std::cerr << "stbi error: " << stbi_failure_reason() << std::endl;
        return false;
    }

    // Validate bounds
    if (startX + regionWidth > imgWidth || startY + regionHeight > imgHeight) {
        std::cerr << "Region out of bounds in sprite sheet (img: " << imgWidth << "x" << imgHeight
                  << ", region: " << startX << "," << startY << " " << regionWidth << "x" << regionHeight << ")" << std::endl;
        stbi_image_free(data);
        return false;
    }

    width = regionWidth;
    height = regionHeight;
    channels = 4;

    // Extract region pixels
    pixels.resize(width * height * channels);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcIdx = ((startY + y) * imgWidth + (startX + x)) * 4;
            int dstIdx = (y * width + x) * 4;
            pixels[dstIdx + 0] = data[srcIdx + 0];  // R
            pixels[dstIdx + 1] = data[srcIdx + 1];  // G
            pixels[dstIdx + 2] = data[srcIdx + 2];  // B
            pixels[dstIdx + 3] = data[srcIdx + 3];  // A
        }
    }

    stbi_image_free(data);

    // Create SDL texture from extracted region
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels.data(), width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32
    );

    if (!surface) {
        std::cerr << "Failed to create surface: " << SDL_GetError() << std::endl;
        return false;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    std::cout << "Loaded sprite region: " << filepath << " at (" << startX << "," << startY
              << ") size " << width << "x" << height << std::endl;
    return true;
}

bool Sprite::isPixelSolid(int x, int y) const {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    if (pixels.empty()) return false;

    int idx = (y * width + x) * channels + 3;  // Alpha channel
    return pixels[idx] > 128;  // Consider solid if alpha > 50%
}

void Sprite::getPixelColor(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b, unsigned char& a) const {
    if (x < 0 || x >= width || y < 0 || y >= height || pixels.empty()) {
        r = g = b = a = 0;
        return;
    }

    int idx = (y * width + x) * channels;
    r = pixels[idx + 0];
    g = pixels[idx + 1];
    b = pixels[idx + 2];
    a = pixels[idx + 3];
}
