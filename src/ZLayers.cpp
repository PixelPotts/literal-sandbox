#include "ZLayers.h"
#include <cmath>
#include <algorithm>

ZLayers::ZLayers() : m_visible(true), m_worldWidth(0), m_viewportHeight(0) {
}

ZLayers::~ZLayers() {
    cleanup();
}

void ZLayers::cleanup() {
    for (auto& layer : m_layers) {
        if (layer.texture) {
            SDL_DestroyTexture(layer.texture);
            layer.texture = nullptr;
        }
    }
    m_layers.clear();
}

void ZLayers::init(SDL_Renderer* renderer, int viewportWidth, int viewportHeight, int worldWidth) {
    cleanup();
    m_worldWidth = worldWidth;
    m_viewportHeight = viewportHeight;

    // Layer at z+5: Far background mountains (furthest back)
    MountainLayer farMountains;
    farMountains.zDepth = 5;
    farMountains.parallaxFactor = 0.15f;
    farMountains.baseColor = {45, 25, 55, 255};
    farMountains.peakColor = {30, 15, 40, 255};
    farMountains.fadeAlpha = 0.5f;
    farMountains.baseY = viewportHeight;
    farMountains.heightScale = 1.4f;
    generateMountainLayer(renderer, farMountains, 42, 0.003f, 1.0f);
    m_layers.push_back(farMountains);

    // Layer at z+4: Closer mountains
    MountainLayer nearMountains;
    nearMountains.zDepth = 4;
    nearMountains.parallaxFactor = 0.3f;
    nearMountains.baseColor = {70, 35, 75, 255};
    nearMountains.peakColor = {50, 25, 60, 255};
    nearMountains.fadeAlpha = 0.65f;
    nearMountains.baseY = viewportHeight;
    nearMountains.heightScale = 1.15f;
    generateMountainLayer(renderer, nearMountains, 137, 0.006f, 1.0f);
    m_layers.push_back(nearMountains);
}

void ZLayers::generateMountainLayer(SDL_Renderer* renderer, MountainLayer& layer, int seed,
                                     float frequency, float amplitude) {
    // Calculate texture dimensions - width based on parallax (less width needed for slower layers)
    int effectiveWidth = (int)(m_worldWidth * layer.parallaxFactor) + m_viewportHeight * 2;
    layer.textureWidth = effectiveWidth;
    layer.textureHeight = m_viewportHeight;

    // Create pixel buffer
    layer.pixels.resize(layer.textureWidth * layer.textureHeight, 0x00000000);

    // Generate mountain for each x position
    for (int x = 0; x < layer.textureWidth; ++x) {
        // Use perlin noise with multiple octaves
        float height = perlinNoise1D(x * frequency, seed, 5, 0.5f);

        // Add sharper peaks
        float peakNoise = perlinNoise1D(x * frequency * 2.0f, seed + 1000, 3, 0.6f);
        if (peakNoise > 0.3f) {
            height += (peakNoise - 0.3f) * 0.5f;
        }

        // Normalize and scale
        height = (height + 1.0f) * 0.5f * amplitude;
        int mountainHeight = (int)(height * m_viewportHeight * layer.heightScale);

        int mountainTop = layer.baseY - mountainHeight;
        int mountainBottom = layer.baseY;

        mountainTop = std::max(0, mountainTop);
        if (mountainTop >= mountainBottom) continue;

        // Draw vertical gradient column
        for (int y = mountainTop; y < mountainBottom && y < layer.textureHeight; ++y) {
            float t = (float)(y - mountainTop) / (float)(mountainBottom - mountainTop);

            Uint8 r = (Uint8)(layer.peakColor.r + t * (layer.baseColor.r - layer.peakColor.r));
            Uint8 g = (Uint8)(layer.peakColor.g + t * (layer.baseColor.g - layer.peakColor.g));
            Uint8 b = (Uint8)(layer.peakColor.b + t * (layer.baseColor.b - layer.peakColor.b));
            Uint8 a = (Uint8)(layer.fadeAlpha * 255);

            layer.pixels[y * layer.textureWidth + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    // Create texture from pixels
    layer.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STATIC,
                                       layer.textureWidth, layer.textureHeight);
    if (layer.texture) {
        SDL_SetTextureBlendMode(layer.texture, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(layer.texture, nullptr, layer.pixels.data(),
                         layer.textureWidth * sizeof(Uint32));
    }

    // Free pixel data after uploading to GPU
    layer.pixels.clear();
    layer.pixels.shrink_to_fit();
}

float ZLayers::noise(float x, int seed) {
    int n = (int)x + seed * 57;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

float ZLayers::smoothNoise(float x, int seed) {
    return noise(x, seed) / 2.0f + noise(x - 1, seed) / 4.0f + noise(x + 1, seed) / 4.0f;
}

float ZLayers::interpolatedNoise(float x, int seed) {
    int intX = (int)x;
    float fracX = x - intX;

    float v1 = smoothNoise((float)intX, seed);
    float v2 = smoothNoise((float)(intX + 1), seed);

    float ft = fracX * 3.14159265f;
    float f = (1.0f - std::cos(ft)) * 0.5f;

    return v1 * (1.0f - f) + v2 * f;
}

float ZLayers::perlinNoise1D(float x, int seed, int octaves, float persistence) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        total += interpolatedNoise(x * frequency, seed + i * 1000) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / maxValue;
}

void ZLayers::render(SDL_Renderer* renderer, float cameraX, float cameraY,
                     int viewportWidth, int viewportHeight,
                     float scaleX, float scaleY) {
    if (!m_visible) return;

    for (const auto& layer : m_layers) {
        if (!layer.texture) continue;

        // Calculate parallax offset
        int srcX = (int)(cameraX * layer.parallaxFactor);

        // Source rectangle (what part of the texture to show)
        SDL_Rect srcRect;
        srcRect.x = srcX;
        srcRect.y = 0;
        srcRect.w = viewportWidth;
        srcRect.h = viewportHeight;

        // Clamp source rect to texture bounds
        if (srcRect.x < 0) srcRect.x = 0;
        if (srcRect.x + srcRect.w > layer.textureWidth) {
            srcRect.w = layer.textureWidth - srcRect.x;
        }

        // Destination rectangle (full screen)
        SDL_Rect dstRect;
        dstRect.x = 0;
        dstRect.y = 0;
        dstRect.w = (int)(srcRect.w * scaleX);
        dstRect.h = (int)(viewportHeight * scaleY);

        SDL_RenderCopy(renderer, layer.texture, &srcRect, &dstRect);
    }
}
