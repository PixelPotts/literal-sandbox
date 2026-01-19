#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <cstdint>

// Represents a single parallax mountain layer
struct MountainLayer {
    int zDepth;                     // Higher = further back (5 = far, 4 = closer)
    float parallaxFactor;           // How much camera movement affects this layer (0.0-1.0)
    std::vector<Uint32> pixels;     // Pre-rendered pixel data (ARGB)
    int textureWidth;               // Width of the layer texture
    int textureHeight;              // Height of the layer texture
    SDL_Texture* texture;           // Pre-rendered texture
    SDL_Color baseColor;            // Base color of mountains
    SDL_Color peakColor;            // Color at peaks (usually darker)
    float fadeAlpha;                // Overall transparency (0.0-1.0)
    int baseY;                      // Y position of mountain base (bottom of mountains)
    float heightScale;              // Multiplier for mountain heights

    MountainLayer() : zDepth(0), parallaxFactor(0.5f), textureWidth(0), textureHeight(0),
                      texture(nullptr), fadeAlpha(1.0f), baseY(0), heightScale(1.0f) {
        baseColor = {80, 40, 80, 255};
        peakColor = {40, 20, 50, 255};
    }
};

class ZLayers {
public:
    ZLayers();
    ~ZLayers();

    // Initialize layers - call after knowing viewport dimensions
    void init(SDL_Renderer* renderer, int viewportWidth, int viewportHeight, int worldWidth);

    // Render all layers behind the main scene
    // Call this BEFORE rendering particles
    void render(SDL_Renderer* renderer, float cameraX, float cameraY,
                int viewportWidth, int viewportHeight,
                float scaleX, float scaleY);

    // Get/set visibility
    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }

    // Cleanup textures
    void cleanup();

private:
    // Generate procedural mountain heights and pre-render to texture
    void generateMountainLayer(SDL_Renderer* renderer, MountainLayer& layer, int seed,
                               float frequency, float amplitude);

    // Simple noise function for mountain generation
    float noise(float x, int seed);
    float smoothNoise(float x, int seed);
    float interpolatedNoise(float x, int seed);
    float perlinNoise1D(float x, int seed, int octaves, float persistence);

    std::vector<MountainLayer> m_layers;
    bool m_visible;
    int m_worldWidth;
    int m_viewportHeight;
};
