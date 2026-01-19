#include "MainSprite.h"
#include "stb_image.h"
#include <iostream>

MainSprite::MainSprite()
    : texture(nullptr)
    , sheetWidth(0)
    , sheetHeight(0)
{
}

MainSprite::~MainSprite() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

bool MainSprite::load(const std::string& filepath, SDL_Renderer* renderer) {
    int imgWidth, imgHeight, imgChannels;
    unsigned char* data = stbi_load(filepath.c_str(), &imgWidth, &imgHeight, &imgChannels, 4);

    if (!data) {
        std::cerr << "MainSprite: Failed to load " << filepath << std::endl;
        std::cerr << "stbi error: " << stbi_failure_reason() << std::endl;
        return false;
    }

    sheetWidth = imgWidth;
    sheetHeight = imgHeight;

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        data, imgWidth, imgHeight, 32, imgWidth * 4, SDL_PIXELFORMAT_RGBA32
    );

    if (!surface) {
        std::cerr << "MainSprite: Failed to create surface: " << SDL_GetError() << std::endl;
        stbi_image_free(data);
        return false;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    stbi_image_free(data);

    if (!texture) {
        std::cerr << "MainSprite: Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    std::cout << "MainSprite: Loaded " << filepath << " (" << sheetWidth << "x" << sheetHeight << ")" << std::endl;
    return true;
}

void MainSprite::defineSprite(const std::string& name, const std::vector<SpriteRegion>& frames) {
    SpriteDefinition def;
    def.name = name;
    def.frames = frames;
    sprites[name] = def;
}

const SpriteDefinition* MainSprite::getSprite(const std::string& name) const {
    auto it = sprites.find(name);
    if (it != sprites.end()) {
        return &it->second;
    }
    return nullptr;
}

void MainSprite::renderFrame(SDL_Renderer* renderer, const std::string& spriteName, int frameIndex,
                              float worldX, float worldY, float cameraX, float cameraY,
                              float scaleX, float scaleY, bool flipHorizontal) {
    if (!texture) return;

    const SpriteDefinition* def = getSprite(spriteName);
    if (!def || def->frames.empty()) return;

    // Clamp frame index
    int idx = frameIndex % (int)def->frames.size();
    const SpriteRegion& frame = def->frames[idx];

    // Source rect from sprite sheet
    SDL_Rect srcRect;
    srcRect.x = frame.x;
    srcRect.y = frame.y;
    srcRect.w = frame.width;
    srcRect.h = frame.height;

    // Destination rect in screen space
    SDL_Rect dstRect;
    dstRect.x = (int)((worldX - cameraX) * scaleX);
    dstRect.y = (int)((worldY - cameraY) * scaleY);
    dstRect.w = (int)(frame.width * scaleX);
    dstRect.h = (int)(frame.height * scaleY);

    SDL_RendererFlip flip = flipHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderCopyEx(renderer, texture, &srcRect, &dstRect, 0.0, nullptr, flip);
}
