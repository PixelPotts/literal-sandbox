#include "World.h"
#include "SandSimulator.h"
#include "Texturize.h"
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <chrono>

// Allow large images (up to 35840x35840 for world images)
#define STBI_MAX_DIMENSIONS 33554432
#include "stb_image.h"

World::World(const Config& cfg) : config(cfg) {
    std::srand(std::time(nullptr));

    // Initialize camera at bottom-left of world
    camera.x = 0;
    camera.y = WORLD_HEIGHT - camera.viewportHeight;  // Bottom of world
    camera.moveSpeed = 25.0f;  // 25 pixels per second as requested

    // Initialize particle chunk system
    particleChunks.resize(P_CHUNKS_X * P_CHUNKS_Y);
    particleChunkActivity.resize(P_CHUNKS_X * P_CHUNKS_Y, false);
}

World::~World() {
    if (sceneImageData) {
        stbi_image_free(sceneImageData);
        sceneImageData = nullptr;
    }
}

void World::moveCamera(float dx, float dy, float deltaTime) {
    float moveAmount = camera.moveSpeed * deltaTime;

    camera.x += dx * moveAmount;
    camera.y += dy * moveAmount;

    // Clamp to world bounds
    camera.x = std::max(0.0f, std::min(camera.x, (float)(WORLD_WIDTH - camera.viewportWidth)));
    camera.y = std::max(0.0f, std::min(camera.y, (float)(WORLD_HEIGHT - camera.viewportHeight)));
}

// World coordinate conversions
void World::worldToChunk(int worldX, int worldY, int& chunkX, int& chunkY) {
    chunkX = worldX / WorldChunk::CHUNK_SIZE;
    chunkY = worldY / WorldChunk::CHUNK_SIZE;
}

void World::worldToParticleChunk(int worldX, int worldY, int& pcX, int& pcY) {
    pcX = worldX / PARTICLE_CHUNK_WIDTH;
    pcY = worldY / PARTICLE_CHUNK_HEIGHT;
}

void World::worldToLocal(int worldX, int worldY, int& localX, int& localY) {
    localX = worldX % WorldChunk::CHUNK_SIZE;
    localY = worldY % WorldChunk::CHUNK_SIZE;
}

void World::chunkToWorld(int chunkX, int chunkY, int& worldX, int& worldY) {
    worldX = chunkX * WorldChunk::CHUNK_SIZE;
    worldY = chunkY * WorldChunk::CHUNK_SIZE;
}

bool World::inWorldBounds(int worldX, int worldY) const {
    return worldX >= 0 && worldX < WORLD_WIDTH && worldY >= 0 && worldY < WORLD_HEIGHT;
}

WorldChunk* World::getChunk(int chunkX, int chunkY) {
    if (chunkX < 0 || chunkX >= WORLD_CHUNKS_X || chunkY < 0 || chunkY >= WORLD_CHUNKS_Y) {
        return nullptr;
    }

    ChunkKey key{chunkX, chunkY};
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        return it->second.get();
    }

    // Create new chunk on demand
    auto chunk = std::make_unique<WorldChunk>(chunkX, chunkY);
    WorldChunk* ptr = chunk.get();
    chunks[key] = std::move(chunk);

    // Populate from scene image if available and not already done
    if (sceneImageData && chunksPopulatedFromScene.find(key) == chunksPopulatedFromScene.end()) {
        populateChunkFromScene(ptr);
        chunksPopulatedFromScene[key] = true;
    }

    procedurallyGenerateMoss(ptr);
    Texturize texturizer;

    // Apply rock texture
    texturizer.applyBrickTexture(this, ptr);

    // Apply obsidian texture
    TextureParams obsidianParams;
    obsidianParams.spawnChance = (config.obsidian.innerRockSpawnChance > 0) ? 1.0f / config.obsidian.innerRockSpawnChance : 0.0f;
    obsidianParams.minPatchSize = config.obsidian.innerRockMinSize;
    obsidianParams.maxPatchSize = config.obsidian.innerRockMaxSize;
    obsidianParams.minPatchRadius = config.obsidian.innerRockMinRadius;
    obsidianParams.maxPatchRadius = config.obsidian.innerRockMaxRadius;
    obsidianParams.colorMultiplier = config.obsidian.innerRockDarkness;
    texturizer.apply(this, ptr, ParticleType::OBSIDIAN, obsidianParams);

    return ptr;
}

bool World::setSceneImage(const std::string& filepath) {
    // Free existing image if any
    if (sceneImageData) {
        stbi_image_free(sceneImageData);
        sceneImageData = nullptr;
    }

    int channels;
    sceneImageData = stbi_load(filepath.c_str(), &sceneImageWidth, &sceneImageHeight, &channels, 3);

    if (!sceneImageData) {
        std::cerr << "Failed to load scene image: " << filepath << std::endl;
        std::cerr << "stbi error: " << stbi_failure_reason() << std::endl;
        return false;
    }

    std::cout << "Scene image loaded: " << filepath << " (" << sceneImageWidth << "x" << sceneImageHeight << ")" << std::endl;
    std::cout << "Image covers world Y range: " << (WORLD_HEIGHT - sceneImageHeight) << " to " << WORLD_HEIGHT << std::endl;
    std::cout << "Image covers world X range: 0 to " << sceneImageWidth << std::endl;

    // Clear the populated tracking so chunks can be repopulated
    chunksPopulatedFromScene.clear();

    return true;
}

void World::populateChunkFromScene(WorldChunk* chunk) {
    if (!sceneImageData || !chunk) return;

    int chunkWorldX = chunk->getWorldX();
    int chunkWorldY = chunk->getWorldY();

    // The image is placed at bottom-left of world
    // Image Y=0 corresponds to world Y = WORLD_HEIGHT - sceneImageHeight
    int imageBaseY = WORLD_HEIGHT - sceneImageHeight;

    // Color mappings
    struct ColorMapping {
        int r, g, b;
        ParticleType type;
    };

    std::vector<ColorMapping> colorMap = {
        // Sand variants
        {255, 200, 100, ParticleType::SAND},
        {194, 178, 128, ParticleType::SAND},  // Tan sand

        // Water variants - multiple blues
        {50, 100, 255, ParticleType::WATER},
        {0, 0, 255, ParticleType::WATER},      // Pure blue
        {0, 100, 255, ParticleType::WATER},    // Azure
        {50, 150, 255, ParticleType::WATER},   // Light blue
        {64, 164, 223, ParticleType::WATER},   // Sky blue

        // Rock
        {128, 128, 128, ParticleType::ROCK},
        {100, 100, 100, ParticleType::ROCK},   // Darker gray
        {150, 150, 150, ParticleType::ROCK},   // Lighter gray

        // Lava
        {255, 100, 0, ParticleType::LAVA},
        {255, 69, 0, ParticleType::LAVA},      // Orange-red

        // Steam
        {240, 240, 240, ParticleType::STEAM},
        {255, 255, 255, ParticleType::STEAM},  // Pure white

        // Obsidian
        {30, 20, 40, ParticleType::OBSIDIAN},

        // Fire
        {255, 50, 0, ParticleType::FIRE},
        {255, 0, 0, ParticleType::FIRE},       // Pure red

        // Ice
        {200, 230, 255, ParticleType::ICE},

        // Glass
        {100, 180, 180, ParticleType::GLASS},
        {0, 255, 255, ParticleType::GLASS},    // Cyan

        // Wood
        {139, 90, 43, ParticleType::WOOD},
        {139, 69, 19, ParticleType::WOOD},      // Saddle brown

        // Moss
        {0, 150, 0, ParticleType::MOSS},
        {20, 130, 20, ParticleType::MOSS}
    };

    auto colorDistance = [](int r1, int g1, int b1, int r2, int g2, int b2) {
        return (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
    };

    // Threshold for color matching - reject blended/anti-aliased pixels
    int threshold = 3500;
    int particlesLoaded = 0;

    for (int localY = 0; localY < WorldChunk::CHUNK_SIZE; localY++) {
        for (int localX = 0; localX < WorldChunk::CHUNK_SIZE; localX++) {
            int worldX = chunkWorldX + localX;
            int worldY = chunkWorldY + localY;

            // Convert world Y to image Y (image starts at bottom-left)
            int imageX = worldX;  // Image X=0 is world X=0
            int imageY = worldY - imageBaseY;  // Offset by image base

            // Check if this position is within the image bounds
            if (imageX < 0 || imageX >= sceneImageWidth || imageY < 0 || imageY >= sceneImageHeight) {
                continue;
            }

            int pixelIdx = (imageY * sceneImageWidth + imageX) * 3;
            int r = sceneImageData[pixelIdx];
            int g = sceneImageData[pixelIdx + 1];
            int b = sceneImageData[pixelIdx + 2];

            // Skip dark pixels (empty/background) - be more aggressive
            if (r < 30 && g < 30 && b < 30) continue;

            ParticleType bestMatch = ParticleType::EMPTY;
            int bestDist = threshold;

            for (const auto& cm : colorMap) {
                int dist = colorDistance(r, g, b, cm.r, cm.g, cm.b);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestMatch = cm.type;
                }
            }

            if (bestMatch != ParticleType::EMPTY) {
                // Set particle directly in chunk
                chunk->setParticle(localX, localY, bestMatch);

                // Set color
                ParticleColor color;
                switch (bestMatch) {
                    case ParticleType::SAND:
                        color = generateRandomColor(config.sand.colorR, config.sand.colorG, config.sand.colorB, config.sand.colorVariation);
                        break;
                    case ParticleType::WATER:
                        color = generateRandomColor(config.water.colorR, config.water.colorG, config.water.colorB, config.water.colorVariation);
                        break;
                    case ParticleType::ROCK:
                        color = generateRandomColor(config.rock.colorR, config.rock.colorG, config.rock.colorB, config.rock.colorVariation);
                        break;
                    case ParticleType::LAVA:
                        color = generateRandomColor(config.lava.colorR, config.lava.colorG, config.lava.colorB, config.lava.colorVariation);
                        break;
                    case ParticleType::STEAM:
                        color = generateRandomColor(config.steam.colorR, config.steam.colorG, config.steam.colorB, config.steam.colorVariation);
                        break;
                    case ParticleType::FIRE:
                        color = generateRandomColor(config.fire.colorR, config.fire.colorG, config.fire.colorB, config.fire.colorVariation);
                        break;
                    case ParticleType::ICE:
                        color = generateRandomColor(config.ice.colorR, config.ice.colorG, config.ice.colorB, config.ice.colorVariation);
                        break;
                    case ParticleType::GLASS:
                        color = generateRandomColor(config.glass.colorR, config.glass.colorG, config.glass.colorB, config.glass.colorVariation);
                        break;
                    case ParticleType::WOOD:
                        color = generateRandomColor(config.wood.colorR, config.wood.colorG, config.wood.colorB, config.wood.colorVariation);
                        break;
                    case ParticleType::OBSIDIAN:
                        color = generateRandomColor(config.obsidian.colorR, config.obsidian.colorG, config.obsidian.colorB, config.obsidian.colorVariation);
                        break;
                    case ParticleType::MOSS:
                        color = generateRandomColor(config.moss.colorR, config.moss.colorG, config.moss.colorB, config.moss.colorVariation);
                        break;
                    default:
                        color = {128, 128, 128};
                }
                chunk->setColor(localX, localY, color);

                // Mark as settled so physics colliders work
                chunk->setSettled(localX, localY, true);

                particlesLoaded++;
            }
        }
    }

    if (particlesLoaded > 0) {
        chunk->setActive(true);
        chunk->setSleeping(false);
    }
}

const WorldChunk* World::getChunk(int chunkX, int chunkY) const {
    if (chunkX < 0 || chunkX >= WORLD_CHUNKS_X || chunkY < 0 || chunkY >= WORLD_CHUNKS_Y) {
        return nullptr;
    }

    ChunkKey key{chunkX, chunkY};
    auto it = chunks.find(key);
    if (it != chunks.end()) {
        return it->second.get();
    }
    return nullptr;
}

WorldChunk* World::getChunkAtWorldPos(int worldX, int worldY) {
    int chunkX, chunkY;
    worldToChunk(worldX, worldY, chunkX, chunkY);
    return getChunk(chunkX, chunkY);
}

const WorldChunk* World::getChunkAtWorldPos(int worldX, int worldY) const {
    int chunkX, chunkY;
    worldToChunk(worldX, worldY, chunkX, chunkY);
    return getChunk(chunkX, chunkY);
}

ParticleType World::getParticle(int worldX, int worldY) const {
    if (!inWorldBounds(worldX, worldY)) return ParticleType::EMPTY;

    const WorldChunk* chunk = getChunkAtWorldPos(worldX, worldY);
    if (!chunk) return ParticleType::EMPTY;

    int localX, localY;
    worldToLocal(worldX, worldY, localX, localY);
    return chunk->getParticle(localX, localY);
}

void World::setParticle(int worldX, int worldY, ParticleType type) {
    if (!inWorldBounds(worldX, worldY)) return;

    WorldChunk* chunk = getChunkAtWorldPos(worldX, worldY);
    if (!chunk) return;

    int localX, localY;
    worldToLocal(worldX, worldY, localX, localY);
    chunk->setParticle(localX, localY, type);
}

ParticleColor World::getColor(int worldX, int worldY) const {
    if (!inWorldBounds(worldX, worldY)) return {0, 0, 0};

    const WorldChunk* chunk = getChunkAtWorldPos(worldX, worldY);
    if (!chunk) return {0, 0, 0};

    int localX, localY;
    worldToLocal(worldX, worldY, localX, localY);
    return chunk->getColor(localX, localY);
}

void World::setColor(int worldX, int worldY, ParticleColor color) {
    if (!inWorldBounds(worldX, worldY)) return;

    WorldChunk* chunk = getChunkAtWorldPos(worldX, worldY);
    if (!chunk) return;

    int localX, localY;
    worldToLocal(worldX, worldY, localX, localY);
    chunk->setColor(localX, localY, color);
}

bool World::isOccupied(int worldX, int worldY) const {
    return getParticle(worldX, worldY) != ParticleType::EMPTY;
}

// Helper struct for HSL color
struct HSL {
    double h; // Hue [0, 360]
    double s; // Saturation [0, 1]
    double l; // Lightness [0, 1]
};

// Converts RGB to HSL
static HSL rgbToHsl(int r, int g, int b) {
    double rd = (double)r / 255.0;
    double gd = (double)g / 255.0;
    double bd = (double)b / 255.0;
    double max_val = std::max({rd, gd, bd});
    double min_val = std::min({rd, gd, bd});
    double h = 0, s = 0, l = (max_val + min_val) / 2.0;

    if (max_val != min_val) {
        double d = max_val - min_val;
        s = l > 0.5 ? d / (2.0 - max_val - min_val) : d / (max_val + min_val);
        if (max_val == rd) {
            h = (gd - bd) / d + (gd < bd ? 6.0 : 0.0);
        } else if (max_val == gd) {
            h = (bd - rd) / d + 2.0;
        } else if (max_val == bd) {
            h = (rd - gd) / d + 4.0;
        }
        h /= 6.0;
    }
    return {h * 360.0, s, l};
}

// Converts HSL to RGB
static ParticleColor hslToRgb(double h, double s, double l) {
    double r, g, b;
    if (s == 0) {
        r = g = b = l; // achromatic
    } else {
        auto hue2rgb = [](double p, double q, double t) {
            if (t < 0) t += 1;
            if (t > 1) t -= 1;
            if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
            if (t < 1.0/2.0) return q;
            if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
            return p;
        };
        double q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
        double p = 2.0 * l - q;
        double h_norm = h / 360.0;
        r = hue2rgb(p, q, h_norm + 1.0/3.0);
        g = hue2rgb(p, q, h_norm);
        b = hue2rgb(p, q, h_norm - 1.0/3.0);
    }
    return {
        static_cast<unsigned char>(std::max(0.0, std::min(255.0, r * 255))),
        static_cast<unsigned char>(std::max(0.0, std::min(255.0, g * 255))),
        static_cast<unsigned char>(std::max(0.0, std::min(255.0, b * 255)))
    };
}

ParticleColor World::generateRandomColor(int baseR, int baseG, int baseB, int variation) {
    if (variation > 0) {
        HSL hsl = rgbToHsl(baseR, baseG, baseB);

        double random_scalar = ((double)std::rand() / RAND_MAX) * 2.0 - 1.0;

        double lightness_variation = (double)variation / 255.0;
        hsl.l += random_scalar * lightness_variation;

        hsl.l = std::max(0.0, std::min(1.0, hsl.l));

        return hslToRgb(hsl.h, hsl.s, hsl.l);
    } else {
        return {(unsigned char)baseR, (unsigned char)baseG, (unsigned char)baseB};
    }
}

void World::spawnParticleAt(int worldX, int worldY, ParticleType type) {
    if (!inWorldBounds(worldX, worldY)) return;
    if (isOccupied(worldX, worldY)) return;

    WorldChunk* chunk = getChunkAtWorldPos(worldX, worldY);
    if (!chunk) return;

    int localX, localY;
    worldToLocal(worldX, worldY, localX, localY);

    chunk->setParticle(localX, localY, type);

    // Set color based on type
    ParticleColor color;
    switch (type) {
        case ParticleType::SAND:
            color = generateRandomColor(config.sand.colorR, config.sand.colorG, config.sand.colorB, config.sand.colorVariation);
            break;
        case ParticleType::WATER:
            color = generateRandomColor(config.water.colorR, config.water.colorG, config.water.colorB, config.water.colorVariation);
            break;
        case ParticleType::ROCK:
            color = generateRandomColor(config.rock.colorR, config.rock.colorG, config.rock.colorB, config.rock.colorVariation);
            break;
        case ParticleType::LAVA:
            color = generateRandomColor(config.lava.colorR, config.lava.colorG, config.lava.colorB, config.lava.colorVariation);
            break;
        case ParticleType::STEAM:
            color = generateRandomColor(config.steam.colorR, config.steam.colorG, config.steam.colorB, config.steam.colorVariation);
            break;
        case ParticleType::FIRE:
            color = generateRandomColor(config.fire.colorR, config.fire.colorG, config.fire.colorB, config.fire.colorVariation);
            break;
        case ParticleType::ICE:
            color = generateRandomColor(config.ice.colorR, config.ice.colorG, config.ice.colorB, config.ice.colorVariation);
            break;
        case ParticleType::GLASS:
            color = generateRandomColor(config.glass.colorR, config.glass.colorG, config.glass.colorB, config.glass.colorVariation);
            break;
        case ParticleType::WOOD:
            color = generateRandomColor(config.wood.colorR, config.wood.colorG, config.wood.colorB, config.wood.colorVariation);
            break;
        case ParticleType::OBSIDIAN:
            color = generateRandomColor(config.obsidian.colorR, config.obsidian.colorG, config.obsidian.colorB, config.obsidian.colorVariation);
            break;
        case ParticleType::MOSS:
            color = generateRandomColor(config.moss.colorR, config.moss.colorG, config.moss.colorB, config.moss.colorVariation);
            break;
        default:
            color = {128, 128, 128};
    }
    chunk->setColor(localX, localY, color);
    chunk->setSettled(localX, localY, false);


    // Wake world chunk
    wakeChunkAtWorldPos(worldX, worldY);

    // Wake particle chunk
    int pcX, pcY;
    worldToParticleChunk(worldX, worldY, pcX, pcY);
    int pcIndex = pcY * P_CHUNKS_X + pcX;
    if (pcIndex >= 0 && pcIndex < particleChunkActivity.size()) {
        particleChunkActivity[pcIndex] = true;
    }
}

void World::loadChunksAroundCamera() {
    int centerChunkX = (int)(camera.x + camera.viewportWidth / 2) / WorldChunk::CHUNK_SIZE;
    int centerChunkY = (int)(camera.y + camera.viewportHeight / 2) / WorldChunk::CHUNK_SIZE;

    // Load chunks within radius
    for (int dy = -LOAD_RADIUS; dy <= LOAD_RADIUS; dy++) {
        for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++) {
            int cx = centerChunkX + dx;
            int cy = centerChunkY + dy;

            if (cx >= 0 && cx < WORLD_CHUNKS_X && cy >= 0 && cy < WORLD_CHUNKS_Y) {
                getChunk(cx, cy);  // Creates chunk if not exists
            }
        }
    }
}

void World::unloadDistantChunks() {
    int centerChunkX = (int)(camera.x + camera.viewportWidth / 2) / WorldChunk::CHUNK_SIZE;
    int centerChunkY = (int)(camera.y + camera.viewportHeight / 2) / WorldChunk::CHUNK_SIZE;

    // Find chunks to unload
    std::vector<ChunkKey> toRemove;

    for (auto& [key, chunk] : chunks) {
        int distX = std::abs(key.x - centerChunkX);
        int distY = std::abs(key.y - centerChunkY);

        // Unload if far from camera and empty
        if (distX > LOAD_RADIUS + 2 || distY > LOAD_RADIUS + 2) {
            if (chunk->isEmpty()) {
                toRemove.push_back(key);
            }
        }
    }

    for (const auto& key : toRemove) {
        chunks.erase(key);
    }
}

void World::getVisibleRegion(int& startX, int& startY, int& endX, int& endY) const {
    startX = (int)camera.x;
    startY = (int)camera.y;
    endX = startX + camera.viewportWidth;
    endY = startY + camera.viewportHeight;

    // Clamp to world bounds
    startX = std::max(0, startX);
    startY = std::max(0, startY);
    endX = std::min(WORLD_WIDTH, endX);
    endY = std::min(WORLD_HEIGHT, endY);
}

bool World::canMoveTo(int worldX, int worldY) const {
    if (!inWorldBounds(worldX, worldY)) return false;
    return !isOccupied(worldX, worldY);
}

void World::moveParticle(int fromX, int fromY, int toX, int toY) {
    if (!inWorldBounds(fromX, fromY) || !inWorldBounds(toX, toY)) return;

    WorldChunk* fromChunk = getChunkAtWorldPos(fromX, fromY);
    WorldChunk* toChunk = getChunkAtWorldPos(toX, toY);

    if (!fromChunk || !toChunk) return;

    int fromLocalX, fromLocalY, toLocalX, toLocalY;
    worldToLocal(fromX, fromY, fromLocalX, fromLocalY);
    worldToLocal(toX, toY, toLocalX, toLocalY);

    // Copy particle data
    ParticleType type = fromChunk->getParticle(fromLocalX, fromLocalY);
    ParticleColor color = fromChunk->getColor(fromLocalX, fromLocalY);
    ParticleVelocity vel = fromChunk->getVelocity(fromLocalX, fromLocalY);
    float temp = fromChunk->getTemperature(fromLocalX, fromLocalY);

    // Clear source
    fromChunk->setParticle(fromLocalX, fromLocalY, ParticleType::EMPTY);
    fromChunk->setColor(fromLocalX, fromLocalY, {0, 0, 0});
    fromChunk->setVelocity(fromLocalX, fromLocalY, {0, 0});

    // Set destination
    toChunk->setParticle(toLocalX, toLocalY, type);
    toChunk->setColor(toLocalX, toLocalY, color);
    toChunk->setVelocity(toLocalX, toLocalY, vel);
    toChunk->setTemperature(toLocalX, toLocalY, temp);
    toChunk->setSettled(toLocalX, toLocalY, false);
    toChunk->setMovedThisFrame(toLocalX, toLocalY, true);

    // Wake world chunks
    wakeChunkAtWorldPos(fromX, fromY);
    wakeChunkAtWorldPos(toX, toY);

    // Wake particle chunks
    int pcX, pcY;
    worldToParticleChunk(fromX, fromY, pcX, pcY);
    particleChunkActivity[pcY * P_CHUNKS_X + pcX] = true;
    worldToParticleChunk(toX, toY, pcX, pcY);
    particleChunkActivity[pcY * P_CHUNKS_X + pcX] = true;
}

void World::swapParticles(int x1, int y1, int x2, int y2) {
    if (!inWorldBounds(x1, y1) || !inWorldBounds(x2, y2)) return;

    WorldChunk* chunk1 = getChunkAtWorldPos(x1, y1);
    WorldChunk* chunk2 = getChunkAtWorldPos(x2, y2);

    if (!chunk1 || !chunk2) return;

    int local1X, local1Y, local2X, local2Y;
    worldToLocal(x1, y1, local1X, local1Y);
    worldToLocal(x2, y2, local2X, local2Y);

    // Get data from both
    ParticleType type1 = chunk1->getParticle(local1X, local1Y);
    ParticleColor color1 = chunk1->getColor(local1X, local1Y);
    ParticleVelocity vel1 = chunk1->getVelocity(local1X, local1Y);

    ParticleType type2 = chunk2->getParticle(local2X, local2Y);
    ParticleColor color2 = chunk2->getColor(local2X, local2Y);
    ParticleVelocity vel2 = chunk2->getVelocity(local2X, local2Y);

    // Swap
    chunk1->setParticle(local1X, local1Y, type2);
    chunk1->setColor(local1X, local1Y, color2);
    chunk1->setVelocity(local1X, local1Y, vel2);
    chunk1->setMovedThisFrame(local1X, local1Y, true);
    chunk1->setSettled(local1X, local1Y, false);

    chunk2->setParticle(local2X, local2Y, type1);
    chunk2->setColor(local2X, local2Y, color1);
    chunk2->setVelocity(local2X, local2Y, vel1);
    chunk2->setMovedThisFrame(local2X, local2Y, true);
    chunk2->setSettled(local2X, local2Y, false);

    // Wake world chunks
    wakeChunkAtWorldPos(x1, y1);
    wakeChunkAtWorldPos(x2, y2);

    // Wake particle chunks
    int pcX, pcY;
    worldToParticleChunk(x1, y1, pcX, pcY);
    particleChunkActivity[pcY * P_CHUNKS_X + pcX] = true;
    worldToParticleChunk(x2, y2, pcX, pcY);
    particleChunkActivity[pcY * P_CHUNKS_X + pcX] = true;
}

void World::wakeChunkAtWorldPos(int worldX, int worldY) {
    WorldChunk* chunk = getChunkAtWorldPos(worldX, worldY);
    if (chunk) {
        chunk->setSleeping(false);
        chunk->setActive(true);
        chunk->resetStableFrames();
    }
}

// Helper to mark a particle as settled
void World::markSettled(int x, int y, bool settled) {
    WorldChunk* chunk = getChunkAtWorldPos(x, y);
    if (!chunk) return;
    int localX, localY;
    worldToLocal(x, y, localX, localY);
    chunk->setSettled(localX, localY, settled);
}

// Particle physics
void World::updateSandParticle(int x, int y) {
    if (y + 1 >= WORLD_HEIGHT) {
        markSettled(x, y, true);
        return;
    }

    // 1. Fall straight down
    ParticleType below = getParticle(x, y + 1);
    if (below == ParticleType::EMPTY) {
        moveParticle(x, y, x, y + 1);
        return;
    }
    if (below == ParticleType::WATER) {
        swapParticles(x, y, x, y + 1);
        return;
    }

    // 2. Diagonal falling
    bool leftOpen = (x > 0 && (getParticle(x - 1, y + 1) == ParticleType::EMPTY || getParticle(x - 1, y + 1) == ParticleType::WATER));
    bool rightOpen = (x < WORLD_WIDTH - 1 && (getParticle(x + 1, y + 1) == ParticleType::EMPTY || getParticle(x + 1, y + 1) == ParticleType::WATER));

    if (leftOpen && rightOpen) {
        int newX = (std::rand() % 2) ? x - 1 : x + 1;
        if (getParticle(newX, y + 1) == ParticleType::EMPTY) {
            moveParticle(x, y, newX, y + 1);
        } else {
            swapParticles(x, y, newX, y + 1);
        }
    } else if (leftOpen) {
        if (getParticle(x - 1, y + 1) == ParticleType::EMPTY) {
            moveParticle(x, y, x - 1, y + 1);
        } else {
            swapParticles(x, y, x - 1, y + 1);
        }
    } else if (rightOpen) {
        if (getParticle(x + 1, y + 1) == ParticleType::EMPTY) {
            moveParticle(x, y, x + 1, y + 1);
        } else {
            swapParticles(x, y, x + 1, y + 1);
        }
    } else {
        // Can't move - mark as settled
        markSettled(x, y, true);
    }
}

void World::updateWaterParticle(int x, int y) {
    // 1. Fall straight down
    if (y + 1 < WORLD_HEIGHT && getParticle(x, y + 1) == ParticleType::EMPTY) {
        moveParticle(x, y, x, y + 1);
        return;
    }

    // 2. Diagonal falling
    bool leftDiag = (x > 0 && y + 1 < WORLD_HEIGHT && getParticle(x - 1, y + 1) == ParticleType::EMPTY);
    bool rightDiag = (x < WORLD_WIDTH - 1 && y + 1 < WORLD_HEIGHT && getParticle(x + 1, y + 1) == ParticleType::EMPTY);

    if (leftDiag && rightDiag) {
        int newX = (std::rand() % 2) ? x - 1 : x + 1;
        moveParticle(x, y, newX, y + 1);
        return;
    }
    if (leftDiag) {
        moveParticle(x, y, x - 1, y + 1);
        return;
    }
    if (rightDiag) {
        moveParticle(x, y, x + 1, y + 1);
        return;
    }

    // 3. Horizontal flow (with path checking)
    int flowSpeed = config.water.horizontalFlowSpeed;
    if (flowSpeed <= 0) flowSpeed = 1; // Ensure at least 1 step

    // Randomly choose left or right preference
    bool preferLeft = (std::rand() % 2 == 0);

    for (int speed = flowSpeed; speed >= 1; --speed) { // Check furthest distances first
        // Try left
        if (preferLeft) {
            if (x - speed >= 0) {
                bool pathClear = true;
                for (int s_path = 1; s_path < speed; ++s_path) {
                    if (getParticle(x - s_path, y) != ParticleType::WATER) { // Path must be water
                        pathClear = false;
                        break;
                    }
                }
                if (pathClear && getParticle(x - speed, y) == ParticleType::EMPTY) {
                    moveParticle(x, y, x - speed, y);
                    return;
                }
            }
        }
        // Try right
        if (x + speed < WORLD_WIDTH) {
            bool pathClear = true;
            for (int s_path = 1; s_path < speed; ++s_path) {
                if (getParticle(x + s_path, y) != ParticleType::WATER) { // Path must be water
                    pathClear = false;
                    break;
                }
            }
            if (pathClear && getParticle(x + speed, y) == ParticleType::EMPTY) {
                moveParticle(x, y, x + speed, y);
                return;
            }
        }

        // If preferLeft was true, now try right if it wasn't already checked
        if (preferLeft) {
            // This part is already covered by the previous if (x + speed < WORLD_WIDTH)
        }
        // If preferLeft was false, now try left if it wasn't already checked
        else {
            if (x - speed >= 0) {
                bool pathClear = true;
                for (int s_path = 1; s_path < speed; ++s_path) {
                    if (getParticle(x - s_path, y) != ParticleType::WATER) { // Path must be water
                        pathClear = false;
                        break;
                    }
                }
                if (pathClear && getParticle(x - speed, y) == ParticleType::EMPTY) {
                    moveParticle(x, y, x - speed, y);
                    return;
                }
            }
        }
    }

    // 4. If no movement possible, mark as settled
    markSettled(x, y, true);
}

void World::updateLavaParticle(int x, int y) {
    // Similar to water but slower
    if (y + 1 < WORLD_HEIGHT && !isOccupied(x, y + 1)) {
        moveParticle(x, y, x, y + 1);
        return;
    }

    // Diagonal
    bool leftDiag = (x > 0 && y + 1 < WORLD_HEIGHT && !isOccupied(x - 1, y + 1));
    bool rightDiag = (x < WORLD_WIDTH - 1 && y + 1 < WORLD_HEIGHT && !isOccupied(x + 1, y + 1));

    if (leftDiag && rightDiag) {
        int newX = (std::rand() % 2) ? x - 1 : x + 1;
        moveParticle(x, y, newX, y + 1);
        return;
    } else if (leftDiag) {
        moveParticle(x, y, x - 1, y + 1);
        return;
    } else if (rightDiag) {
        moveParticle(x, y, x + 1, y + 1);
        return;
    }

    // Slow horizontal flow
    bool leftOpen = (x - 1 >= 0) && !isOccupied(x - 1, y);
    bool rightOpen = (x + 1 < WORLD_WIDTH) && !isOccupied(x + 1, y);

    if (leftOpen && rightOpen && (std::rand() % 3 == 0)) {
        int newX = (std::rand() % 2) ? x - 1 : x + 1;
        moveParticle(x, y, newX, y);
        return;
    } else if (leftOpen && (std::rand() % 3 == 0)) {
        moveParticle(x, y, x - 1, y);
        return;
    } else if (rightOpen && (std::rand() % 3 == 0)) {
        moveParticle(x, y, x + 1, y);
        return;
    }

    // Can't move - mark as settled (lava solidifies quickly)
    markSettled(x, y, true);
}

void World::updateSteamParticle(int x, int y) {
    // Rise up
    if (y > 0 && !isOccupied(x, y - 1)) {
        moveParticle(x, y, x, y - 1);
        return;
    }

    // Diagonal rising
    bool leftUp = (x > 0 && y > 0 && !isOccupied(x - 1, y - 1));
    bool rightUp = (x < WORLD_WIDTH - 1 && y > 0 && !isOccupied(x + 1, y - 1));

    if (leftUp && rightUp) {
        int newX = (std::rand() % 2) ? x - 1 : x + 1;
        moveParticle(x, y, newX, y - 1);
    } else if (leftUp) {
        moveParticle(x, y, x - 1, y - 1);
    } else if (rightUp) {
        moveParticle(x, y, x + 1, y - 1);
    }
}

void World::updateFireParticle(int x, int y) {
    // Fire rises and flickers
    if (y > 0 && !isOccupied(x, y - 1) && (std::rand() % 2 == 0)) {
        moveParticle(x, y, x, y - 1);
        return;
    }

    // Random sideways movement
    if (std::rand() % 3 == 0) {
        int dx = (std::rand() % 3) - 1;  // -1, 0, or 1
        int newX = x + dx;
        if (newX >= 0 && newX < WORLD_WIDTH && !isOccupied(newX, y)) {
            moveParticle(x, y, newX, y);
        }
    }
}

void World::updateIceParticle(int x, int y) {
    // Ice doesn't move (static) - always settled
    markSettled(x, y, true);
}

void World::updateMossParticle(int x, int y) {
    // Moss is mostly static
    markSettled(x, y, true);
}




void World::updateWetnessForParticle(int x, int y) {
    ParticleType type = getParticle(x, y);
    if (type == ParticleType::EMPTY || type == ParticleType::WATER) {
        return;
    }

    // Absorption
    float currentWetness = getWetness(x, y);
    float maxSaturation = getMaxSaturation(type);
    if (currentWetness < maxSaturation) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx;
                int ny = y + dy;
                if (inWorldBounds(nx, ny) && getParticle(nx, ny) == ParticleType::WATER) {
                    float absorbAmount = config.wetnessAbsorptionRate * (maxSaturation - currentWetness);
                    setWetness(x, y, currentWetness + absorbAmount);
                    // For simplicity, we don't remove the water particle.
                    // In a more complex simulation, we might.
                }
            }
        }
    }

    // Spreading
    currentWetness = getWetness(x, y);
    if (currentWetness > config.wetnessMinimumThreshold) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx;
                int ny = y + dy;
                if (inWorldBounds(nx, ny)) {
                    ParticleType neighborType = getParticle(nx, ny);
                    if (neighborType != ParticleType::EMPTY && neighborType != ParticleType::WATER) {
                        float neighborWetness = getWetness(nx, ny);
                        float maxNeighborSaturation = getMaxSaturation(neighborType);
                        if (neighborWetness < maxNeighborSaturation && currentWetness > neighborWetness) {
                            float transferAmount = (currentWetness - neighborWetness) * config.wetnessSpreadRate;
                            setWetness(x, y, currentWetness - transferAmount);
                            setWetness(nx, ny, neighborWetness + transferAmount);
                        }
                    }
                }
            }
        }
    }
}

void World::updateParticle(int worldX, int worldY) {
    ParticleType type = getParticle(worldX, worldY);

    switch (type) {
        case ParticleType::SAND:
            updateSandParticle(worldX, worldY);
            break;
        case ParticleType::WATER:
            updateWaterParticle(worldX, worldY);
            break;
        case ParticleType::LAVA:
            updateLavaParticle(worldX, worldY);
            break;
        case ParticleType::STEAM:
            updateSteamParticle(worldX, worldY);
            break;
        case ParticleType::FIRE:
            updateFireParticle(worldX, worldY);
            break;
        case ParticleType::ICE:
            updateIceParticle(worldX, worldY);
            break;
        case ParticleType::MOSS:
            updateMossParticle(worldX, worldY);
            break;
        default:
            break;
    }
}



void World::update(float deltaTime) {
    loadChunksAroundCamera();
    unloadDistantChunks();

    std::fill(particleChunkActivity.begin(), particleChunkActivity.end(), false);

    // Determine visible particle chunk range to simulate
    int visWorldX_start, visWorldY_start, visWorldX_end, visWorldY_end;
    getVisibleRegion(visWorldX_start, visWorldY_start, visWorldX_end, visWorldY_end);

    int startPCX, startPCY, endPCX, endPCY;
    worldToParticleChunk(visWorldX_start, visWorldY_start, startPCX, startPCY);
    worldToParticleChunk(visWorldX_end - 1, visWorldY_end - 1, endPCX, endPCY);

    // Simulate a border of 1 particle chunk around the visible area
    startPCX = std::max(0, startPCX - 1);
    startPCY = std::max(0, startPCY - 1);
    endPCX = std::min(P_CHUNKS_X - 1, endPCX + 1);
    endPCY = std::min(P_CHUNKS_Y - 1, endPCY + 1);

    for (int pcY = endPCY; pcY >= startPCY; --pcY) {
        bool leftToRight = (pcY % 2 == 0);
        for (int i = 0; i < (endPCX - startPCX + 1); ++i) {
            int pcX = startPCX + (leftToRight ? i : (endPCX - startPCX - i));

            int pcIndex = pcY * P_CHUNKS_X + pcX;
            if (!particleChunks[pcIndex].isAwake) {
                continue;
            }

            int startWorldX = pcX * PARTICLE_CHUNK_WIDTH;
            int startWorldY = pcY * PARTICLE_CHUNK_HEIGHT;
            int endWorldX = startWorldX + PARTICLE_CHUNK_WIDTH;
            int endWorldY = startWorldY + PARTICLE_CHUNK_HEIGHT;

            for (int y = endWorldY - 1; y >= startWorldY; --y) {
                 if (y < 0 || y >= WORLD_HEIGHT) continue;
                for (int x = startWorldX; x < endWorldX; ++x) {
                    if (x < 0 || x >= WORLD_WIDTH) continue;
                    
                    ParticleType type = getParticle(x, y);
                    if (type != ParticleType::EMPTY) {
                        updateParticle(x, y);
                    }
                }
            }
        }
    }

    // Update sleep states
    for (int i = 0; i < particleChunks.size(); ++i) {
        if (particleChunkActivity[i]) {
            particleChunks[i].isAwake = true;
            particleChunks[i].stableFrames = 0;
            // Wake up neighbors
            int pcX = i % P_CHUNKS_X;
            int pcY = i / P_CHUNKS_X;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    int nPcX = pcX + dx;
                    int nPcY = pcY + dy;
                    if (nPcX >= 0 && nPcX < P_CHUNKS_X && nPcY >= 0 && nPcY < P_CHUNKS_Y) {
                        int neighborIdx = nPcY * P_CHUNKS_X + nPcX;
                        particleChunks[neighborIdx].isAwake = true;
                        particleChunks[neighborIdx].stableFrames = 0;
                    }
                }
            }
        } else {
            if (particleChunks[i].isAwake) {
                particleChunks[i].stableFrames++;
                if (particleChunks[i].stableFrames > P_CHUNK_FRAMES_UNTIL_SLEEP) {
                    particleChunks[i].isAwake = false;
                }
            }
        }
    }
}

bool World::loadSceneFromBMP(const std::string& filepath, int worldOffsetX, int worldOffsetY) {
    int imgWidth, imgHeight, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &imgWidth, &imgHeight, &channels, 3);

    if (!data) {
        std::cerr << "Failed to load scene: " << filepath << std::endl;
        return false;
    }

    std::cout << "Loading scene " << filepath << " (" << imgWidth << "x" << imgHeight << ")" << std::endl;

    // Color mappings
    struct ColorMapping {
        int r, g, b;
        ParticleType type;
    };

    std::vector<ColorMapping> colorMap = {
        {255, 200, 100, ParticleType::SAND},
        {50, 100, 255, ParticleType::WATER},
        {128, 128, 128, ParticleType::ROCK},
        {255, 100, 0, ParticleType::LAVA},
        {240, 240, 240, ParticleType::STEAM},
        {30, 20, 40, ParticleType::OBSIDIAN},
        {255, 50, 0, ParticleType::FIRE},
        {200, 230, 255, ParticleType::ICE},
        {100, 180, 180, ParticleType::GLASS},
        {139, 90, 43, ParticleType::WOOD},
        {0, 150, 0, ParticleType::MOSS}
    };

    auto colorDistance = [](int r1, int g1, int b1, int r2, int g2, int b2) {
        return (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
    };

    int threshold = 5000;
    int particlesLoaded = 0;

    for (int iy = 0; iy < imgHeight; iy++) {
        for (int ix = 0; ix < imgWidth; ix++) {
            int worldX = worldOffsetX + ix;
            int worldY = worldOffsetY + iy;

            if (!inWorldBounds(worldX, worldY)) continue;

            int pixelIdx = (iy * imgWidth + ix) * 3;
            int r = data[pixelIdx];
            int g = data[pixelIdx + 1];
            int b = data[pixelIdx + 2];

            if (r < 10 && g < 10 && b < 10) continue;

            ParticleType bestMatch = ParticleType::EMPTY;
            int bestDist = threshold;

            for (const auto& cm : colorMap) {
                int dist = colorDistance(r, g, b, cm.r, cm.g, cm.b);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestMatch = cm.type;
                }
            }

            if (bestMatch != ParticleType::EMPTY) {
                spawnParticleAt(worldX, worldY, bestMatch);
                particlesLoaded++;
            }
        }
    }

    stbi_image_free(data);
    std::cout << "Loaded " << particlesLoaded << " particles from scene" << std::endl;
    return true;
}

int World::getParticleCount(ParticleType type) const {
    int count = 0;
    for (const auto& [key, chunk] : chunks) {
        if (!chunk) continue;

        const auto& particles = chunk->getParticleGrid();
        for (const auto& p : particles) {
            if (p == type) count++;
        }
    }
    return count;
}

void World::addSceneObject(std::shared_ptr<SceneObject> obj) {
    if (obj) {
        sceneObjects.push_back(obj);
    }
}

void World::removeSceneObject(SceneObject* obj) {
    sceneObjects.erase(
        std::remove_if(sceneObjects.begin(), sceneObjects.end(),
            [obj](const std::shared_ptr<SceneObject>& o) { return o.get() == obj; }),
        sceneObjects.end()
    );
}

bool World::isBlockedBySceneObject(int worldX, int worldY) const {
    for (const auto& obj : sceneObjects) {
        if (!obj || !obj->isActive() || !obj->blocksParticles()) continue;
        if (obj->isPixelSolidAt(worldX, worldY)) {
            return true;
        }
    }
    return false;
}

float World::getParticleMass(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND:     return config.sand.mass;      // 2.0
        case ParticleType::WATER:    return config.water.mass;     // 1.0
        case ParticleType::ROCK:     return config.rock.mass;      // 5.0
        case ParticleType::WOOD:     return config.wood.mass;      // 0.6
        case ParticleType::LAVA:     return config.lava.mass;      // 2.5
        case ParticleType::STEAM:    return config.steam.mass;     // -1.0 (rises)
        case ParticleType::OBSIDIAN: return config.obsidian.mass;  // 6.0
        case ParticleType::FIRE:     return config.fire.mass;      // -0.3 (rises)
        case ParticleType::ICE:      return config.ice.mass;       // 0.9
        case ParticleType::GLASS:    return config.glass.mass;     // 2.5
        default: return 1.0f;
    }
}

void World::explodeAt(int worldX, int worldY, int radius, float force) {
    // Set velocity on all particles in radius - physics system will move them
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int px = worldX + dx;
            int py = worldY + dy;

            if (!inWorldBounds(px, py)) continue;

            float dist = std::sqrt((float)(dx * dx + dy * dy));
            if (dist > radius) continue;

            WorldChunk* chunk = getChunkAtWorldPos(px, py);
            if (!chunk) continue;

            int localX, localY;
            worldToLocal(px, py, localX, localY);

            ParticleType type = chunk->getParticle(localX, localY);
            if (type == ParticleType::EMPTY) continue;

            // Skip completely immovable particles
            if (type == ParticleType::OBSIDIAN) continue;

            // Get mass - lighter particles fly further
            float mass = std::abs(getParticleMass(type));
            if (mass < 0.1f) mass = 0.1f; // Prevent division issues

            // Mass multiplier: lighter = higher multiplier, heavy = much lower
            // Use squared inverse for heavier materials to make them barely move
            float massMultiplier;
            if (mass <= 1.0f) {
                // Light materials: linear inverse (mass 0.3 -> 3.3x, mass 1.0 -> 1x)
                massMultiplier = 1.0f / mass;
            } else {
                // Heavy materials: squared inverse (mass 2 -> 0.25x, mass 5 -> 0.04x)
                massMultiplier = 1.0f / (mass * mass);
            }

            // Calculate outward velocity - stronger near center
            float falloff = 1.0f - (dist / (float)radius);
            falloff = falloff * falloff; // Quadratic falloff

            float dirX = (dist > 0.1f) ? dx / dist : 0.0f;
            float dirY = (dist > 0.1f) ? dy / dist : -1.0f;

            // Add randomness
            float randAngle = ((std::rand() % 100) - 50) / 100.0f * 0.6f;
            float cosA = std::cos(randAngle);
            float sinA = std::sin(randAngle);
            float newDirX = dirX * cosA - dirY * sinA;
            float newDirY = dirX * sinA + dirY * cosA;

            float randForce = 0.7f + ((std::rand() % 60) / 100.0f);

            // Set velocity - scaled by inverse mass (lighter = faster)
            ParticleVelocity vel;
            vel.vx = newDirX * force * falloff * randForce * massMultiplier;
            vel.vy = newDirY * force * falloff * randForce * massMultiplier;

            chunk->setVelocity(localX, localY, vel);
            chunk->setExploding(localX, localY, true);  // Set exploding mode
            // Random timeout: -60 to -30 frames = 500-1000ms at 60fps, deleted when age >= 0
            chunk->setParticleAge(localX, localY, -(30 + (std::rand() % 31)));

            // Wake chunk
            chunk->setSleeping(false);
            chunk->setActive(true);
            chunk->resetStableFrames();
        }
    }
}

bool World::isSolidParticle(ParticleType type) const {
    return type == ParticleType::ROCK ||
           type == ParticleType::WOOD ||
           type == ParticleType::OBSIDIAN ||
           type == ParticleType::GLASS ||
           type == ParticleType::ICE ||
           type == ParticleType::MOSS;
}

float World::getWetness(int worldX, int worldY) const {
    if (!inWorldBounds(worldX, worldY)) return 0.0f;
    const WorldChunk* chunk = getChunkAtWorldPos(worldX, worldY);
    if (!chunk) return 0.0f;
    int localX, localY;
    worldToLocal(worldX, worldY, localX, localY);
    return chunk->getWetness(localX, localY);
}

void World::setWetness(int worldX, int worldY, float wetness) {
    if (!inWorldBounds(worldX, worldY)) return;
    WorldChunk* chunk = getChunkAtWorldPos(worldX, worldY);
    if (!chunk) return;
    int localX, localY;
    worldToLocal(worldX, worldY, localX, localY);
    chunk->setWetness(localX, localY, wetness);
}

float World::getMaxSaturation(ParticleType type) const {
    switch (type) {
        case ParticleType::SAND: return config.sand.maxSaturation;
        case ParticleType::WOOD: return config.wood.maxSaturation;
        case ParticleType::MOSS: return config.moss.maxSaturation;
        default: return 0.0f;
    }
}


bool World::checkCapsuleCollision(float centerX, float centerY, float radius, float height, float& collisionY) const {
    collisionY = WORLD_HEIGHT; // Initialize to a large value
    bool collided = false;

    // centerY is the center of the top circle
    float topCircleY = centerY;
    float botCircleY = centerY + height;

    // Check points along the capsule perimeter
    // Top semicircle
    for (int angle = 0; angle <= 180; angle += 15) {
        float rad = angle * 3.14159f / 180.0f;
        int px = (int)(centerX + std::cos(rad) * radius);
        int py = (int)(topCircleY - std::sin(rad) * radius);
        if (inWorldBounds(px, py) && isSolidParticle(getParticle(px, py))) {
            collided = true;
            if (py < collisionY) {
                collisionY = py;
            }
        }
    }

    // Bottom semicircle
    for (int angle = 0; angle <= 180; angle += 15) {
        float rad = angle * 3.14159f / 180.0f;
        int px = (int)(centerX + std::cos(rad) * radius);
        int py = (int)(botCircleY + std::sin(rad) * radius);
        if (inWorldBounds(px, py) && isSolidParticle(getParticle(px, py))) {
            collided = true;
            if (py < collisionY) {
                collisionY = py;
            }
        }
    }

    // Left and right edges of the body
    for (float y = topCircleY; y <= botCircleY; y += 2.0f) {
        int leftX = (int)(centerX - radius);
        int rightX = (int)(centerX + radius);
        int py = (int)y;

        if (inWorldBounds(leftX, py) && isSolidParticle(getParticle(leftX, py))) {
            collided = true;
            if (py < collisionY) {
                collisionY = py;
            }
        }
        if (inWorldBounds(rightX, py) && isSolidParticle(getParticle(rightX, py))) {
            collided = true;
            if (py < collisionY) {
                collisionY = py;
            }
        }
    }

    return collided;
}


void World::procedurallyGenerateMoss(WorldChunk* chunk) {
    if (!chunk) return;

    // A time limit for generation to stop after a few seconds from the start of the program
    // static std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
    // if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(2)) {
    //     return;
    // }

    int chunkWorldX = chunk->getWorldX();
    int chunkWorldY = chunk->getWorldY();

    for (int y = 0; y < WorldChunk::CHUNK_SIZE; ++y) {
        for (int x = 0; x < WorldChunk::CHUNK_SIZE; ++x) {
            int worldX = chunkWorldX + x;
            int worldY = chunkWorldY + y;

            // If this particle is rock and the one above it is empty
            if (getParticle(worldX, worldY) == ParticleType::ROCK && getParticle(worldX, worldY - 1) == ParticleType::EMPTY) {
                if (std::rand() % 10 < 1) {
                    int width = 2 + std::rand() % 8;
                    int depth = 1 + std::rand() % 4;

                    // for the depth we want to make the moss
                    for (int py = -1; py < depth; ++py) {

                        // for the width we want to make the moss
                        for (int px = -width / 2; px < width / 2; ++px) {
                            int mossX = worldX + px;
                            int mossY = worldY + py;

                            // generate the moss, replace the rock particles if they're on it
                            if (inWorldBounds(mossX, mossY)) {
                                ParticleType existingParticle = getParticle(mossX, mossY);
                                if (existingParticle == ParticleType::ROCK) {
                                    setParticle(mossX, mossY, ParticleType::MOSS);
                                    setColor(mossX, mossY, generateRandomColor(config.moss.colorR, config.moss.colorG, config.moss.colorB, config.moss.colorVariation));

                                // some particles grow above the rock
                                } else if (existingParticle == ParticleType::EMPTY) {
                                    ParticleType particleBelow = getParticle(mossX, mossY + 1);
                                    if (particleBelow == ParticleType::ROCK || particleBelow == ParticleType::MOSS) {
                                        setParticle(mossX, mossY, ParticleType::MOSS);
                                        setColor(mossX, mossY, generateRandomColor(config.moss.colorR, config.moss.colorG, config.moss.colorB, config.moss.colorVariation));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
