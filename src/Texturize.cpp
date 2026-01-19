#include "Texturize.h"
#include "World.h"
#include "WorldChunk.h"
#include "Config.h"
#include <cstdlib>
#include <algorithm>
#include <cmath>

void Texturize::apply(World* world, WorldChunk* chunk, ParticleType targetType, const TextureParams& params) {
    if (!world || !chunk) return;

    const Config& config = world->getConfig();

    int chunkWorldX = chunk->getWorldX();
    int chunkWorldY = chunk->getWorldY();

    for (int y = 0; y < WorldChunk::CHUNK_SIZE; ++y) {
        for (int x = 0; x < WorldChunk::CHUNK_SIZE; ++x) {
            int worldX = chunkWorldX + x;
            int worldY = chunkWorldY + y;

            ParticleType type = world->getParticle(worldX, worldY);
            if (type == targetType) {
                if (params.spawnChance > 0 && (float)std::rand() / RAND_MAX < params.spawnChance) {
                    int patch_size = params.minPatchSize + std::rand() % (params.maxPatchSize - params.minPatchSize + 1);
                    float patch_radius = params.minPatchRadius + (float)(std::rand()) / (float)(RAND_MAX / (params.maxPatchRadius - params.minPatchRadius));

                    for (int dy = -patch_size / 2; dy <= patch_size / 2; ++dy) {
                        for (int dx = -patch_size / 2; dx <= patch_size / 2; ++dx) {
                            if (dx * dx + dy * dy <= patch_radius * patch_radius) {
                                int currentX = worldX + dx;
                                int currentY = worldY + dy;

                                if (world->inWorldBounds(currentX, currentY) && world->getParticle(currentX, currentY) == targetType) {
                                    ParticleColor color = world->getColor(currentX, currentY);
                                    world->setColor(currentX, currentY, {
                                        static_cast<unsigned char>(color.r * params.colorMultiplier),
                                        static_cast<unsigned char>(color.g * params.colorMultiplier),
                                        static_cast<unsigned char>(color.b * params.colorMultiplier)
                                    });
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void Texturize::applyBrickTexture(World* world, WorldChunk* chunk) {
    if (!world || !chunk) return;

    const Config& config = world->getConfig();
    if (!config.rock.brickTextureEnabled) return;

    const int BRICK_W = config.rock.brickWidth;
    const int BRICK_H = config.rock.brickHeight;
    const int MORTAR_SIZE = config.rock.mortarSize;

    const int TOTAL_BRICK_W = BRICK_W + MORTAR_SIZE;
    const int TOTAL_BRICK_H = BRICK_H + MORTAR_SIZE;

    int chunkWorldX = chunk->getWorldX();
    int chunkWorldY = chunk->getWorldY();

    for (int y = 0; y < WorldChunk::CHUNK_SIZE; ++y) {
        for (int x = 0; x < WorldChunk::CHUNK_SIZE; ++x) {
            int worldX = chunkWorldX + x;
            int worldY = chunkWorldY + y;

            if (world->getParticle(worldX, worldY) != ParticleType::ROCK) {
                continue;
            }
            
            // Overall sparsity check
            if ((float)std::rand() / RAND_MAX > config.rock.overallSparsity) {
                continue;
            }

            // Calculate brick coordinates, considering the offset for alternating rows
            int row = worldY / TOTAL_BRICK_H;
            int brick_x_offset_for_row = (row % 2 == 0) ? 0 : TOTAL_BRICK_W / 2;

            int x_mortar_check = (worldX + brick_x_offset_for_row) % TOTAL_BRICK_W;
            int y_mortar_check = worldY % TOTAL_BRICK_H;
            
            bool is_mortar = (x_mortar_check < MORTAR_SIZE || y_mortar_check < MORTAR_SIZE);

            // Random hash for brick characteristics
            int brick_col = (worldX + brick_x_offset_for_row) / TOTAL_BRICK_W;
            unsigned int brick_hash = (row * 13 + brick_col * 23);
            float brick_rand_main = (float)(brick_hash % 1000) / 1000.0f;
            float brick_rand_type = (float)((brick_hash >> 8) % 1000) / 1000.0f; // Another random for type

            ParticleColor color = world->getColor(worldX, worldY);

            // Integrate long lines into mortar determination
            if (!is_mortar) { // Only check if not already regular mortar
                // Long horizontal lines (check near bottom of brick)
                if (brick_rand_main < config.rock.longLineChance && (y_mortar_check - MORTAR_SIZE) >= BRICK_H - MORTAR_SIZE * 2) {
                    is_mortar = true; // Treat as mortar for this pixel
                }
                // Long vertical lines (check near right of brick)
                if (brick_rand_main > 1.0f - config.rock.longLineChance && (x_mortar_check - MORTAR_SIZE) >= BRICK_W - MORTAR_SIZE * 2) {
                    is_mortar = true; // Treat as mortar for this pixel
                }
            }


            if (is_mortar) {
                world->setColor(worldX, worldY, {
                    static_cast<unsigned char>(static_cast<float>(color.r) * config.rock.mortarColorMultiplier),
                    static_cast<unsigned char>(static_cast<float>(color.g) * config.rock.mortarColorMultiplier),
                    static_cast<unsigned char>(static_cast<float>(color.b) * config.rock.mortarColorMultiplier)
                });
            } else {
                // Now, calculate coordinates within the actual brick (excluding mortar)
                int x_in_brick = x_mortar_check - MORTAR_SIZE;
                int y_in_brick = y_mortar_check - MORTAR_SIZE;
                
                if (brick_rand_type < config.rock.darkBrickChance) { // Dark brick
                    world->setColor(worldX, worldY, { 
                        static_cast<unsigned char>(static_cast<float>(color.r) * config.rock.darkBrickColorMultiplier),
                        static_cast<unsigned char>(static_cast<float>(color.g) * config.rock.darkBrickColorMultiplier),
                        static_cast<unsigned char>(static_cast<float>(color.b) * config.rock.darkBrickColorMultiplier)
                    });
                } else if (brick_rand_type < config.rock.darkBrickChance + config.rock.lightBrickChance) { // Light brick
                     world->setColor(worldX, worldY, { 
                        static_cast<unsigned char>(std::min(255.0f, static_cast<float>(color.r) * config.rock.lightBrickColorMultiplier)), 
                        static_cast<unsigned char>(std::min(255.0f, static_cast<float>(color.g) * config.rock.lightBrickColorMultiplier)), 
                        static_cast<unsigned char>(std::min(255.0f, static_cast<float>(color.b) * config.rock.lightBrickColorMultiplier)) 
                    });
                } else if (brick_rand_type < config.rock.darkBrickChance + config.rock.lightBrickChance + config.rock.borderedBrickChance) { // Bordered brick
                     bool is_outline = (x_in_brick < MORTAR_SIZE || x_in_brick >= BRICK_W - MORTAR_SIZE || y_in_brick < MORTAR_SIZE || y_in_brick >= BRICK_H - MORTAR_SIZE);
                     if(is_outline) {
                        world->setColor(worldX, worldY, { 
                            static_cast<unsigned char>(static_cast<float>(color.r) * config.rock.brickOutlineColorMultiplier), 
                            static_cast<unsigned char>(static_cast<float>(color.g) * config.rock.brickOutlineColorMultiplier), 
                            static_cast<unsigned char>(static_cast<float>(color.b) * config.rock.brickOutlineColorMultiplier) 
                        });
                     }

                     if (brick_rand_type < config.rock.darkBrickChance + config.rock.lightBrickChance + config.rock.borderedBrickChance + config.rock.thickBorderBrickChance) { // Thick border brick
                        bool is_thick_outline = ( (x_in_brick >= BRICK_W - MORTAR_SIZE * 2) || (y_in_brick >= BRICK_H - MORTAR_SIZE * 2) );
                        if(is_thick_outline) {
                            world->setColor(worldX, worldY, { 
                                static_cast<unsigned char>(static_cast<float>(color.r) * config.rock.brickOutlineColorMultiplier), 
                                static_cast<unsigned char>(static_cast<float>(color.g) * config.rock.brickOutlineColorMultiplier), 
                                static_cast<unsigned char>(static_cast<float>(color.b) * config.rock.brickOutlineColorMultiplier) 
                            });
                        }
                     }
                }
                // Else: brick with no border (majority) - do nothing to the color
            }
        }
    }
}

void Texturize::applyRockBorders(World* world, WorldChunk* chunk) {
    if (!world || !chunk) return;

    const Config& config = world->getConfig();
    if (!config.rock.borderEnabled) return;

    int chunkWorldX = chunk->getWorldX();
    int chunkWorldY = chunk->getWorldY();
    int borderWidth = config.rock.borderWidth;
    bool islandExcluded = config.rock.borderIslandExcluded;
    bool ignoreMoss = config.rock.borderIgnoreMoss;

    // Helper to check if a particle counts as "rock" for border purposes
    auto isRockLike = [ignoreMoss](ParticleType type) {
        if (type == ParticleType::ROCK) return true;
        if (ignoreMoss && type == ParticleType::MOSS) return true;
        return false;
    };

    // Expanded area to detect islands - use larger area for better detection
    int expandSize = 64; // Large enough to detect most holes
    int areaSize = WorldChunk::CHUNK_SIZE + expandSize * 2;
    int areaStartX = chunkWorldX - expandSize;
    int areaStartY = chunkWorldY - expandSize;

    // Map to track which non-rock pixels are islands (enclosed by rock)
    // 0 = rock-like, -1 = non-rock unclassified, 1 = exterior, 2 = island
    std::vector<std::vector<int>> regionMap(areaSize, std::vector<int>(areaSize, 0));

    // First pass: mark all non-rock pixels
    for (int y = 0; y < areaSize; ++y) {
        for (int x = 0; x < areaSize; ++x) {
            int worldX = areaStartX + x;
            int worldY = areaStartY + y;

            if (!world->inWorldBounds(worldX, worldY)) {
                regionMap[y][x] = -1; // Out of bounds = non-rock
            } else if (!isRockLike(world->getParticle(worldX, worldY))) {
                regionMap[y][x] = -1; // Non-rock material
            }
        }
    }

    // Seed flood fill from area edges - non-rock at edges extends beyond view = exterior
    std::vector<std::pair<int, int>> queue;
    for (int x = 0; x < areaSize; ++x) {
        if (regionMap[0][x] == -1) queue.push_back({x, 0});
        if (regionMap[areaSize-1][x] == -1) queue.push_back({x, areaSize-1});
    }
    for (int y = 1; y < areaSize - 1; ++y) {
        if (regionMap[y][0] == -1) queue.push_back({0, y});
        if (regionMap[y][areaSize-1] == -1) queue.push_back({areaSize-1, y});
    }

    while (!queue.empty()) {
        auto [x, y] = queue.back();
        queue.pop_back();

        if (x < 0 || x >= areaSize || y < 0 || y >= areaSize) continue;
        if (regionMap[y][x] != -1) continue;

        regionMap[y][x] = 1; // Exterior

        queue.push_back({x-1, y});
        queue.push_back({x+1, y});
        queue.push_back({x, y-1});
        queue.push_back({x, y+1});
    }

    // Any remaining -1 pixels are islands (enclosed non-rock not connected to area edge)
    for (int y = 0; y < areaSize; ++y) {
        for (int x = 0; x < areaSize; ++x) {
            if (regionMap[y][x] == -1) {
                regionMap[y][x] = 2; // Island
            }
        }
    }

    // Build distance map and track if nearest non-rock is an island
    std::vector<std::vector<float>> distanceMap(WorldChunk::CHUNK_SIZE,
        std::vector<float>(WorldChunk::CHUNK_SIZE, static_cast<float>(borderWidth + 1)));
    std::vector<std::vector<bool>> adjacentToIsland(WorldChunk::CHUNK_SIZE,
        std::vector<bool>(WorldChunk::CHUNK_SIZE, false));

    for (int y = 0; y < WorldChunk::CHUNK_SIZE; ++y) {
        for (int x = 0; x < WorldChunk::CHUNK_SIZE; ++x) {
            int worldX = chunkWorldX + x;
            int worldY = chunkWorldY + y;

            if (world->getParticle(worldX, worldY) != ParticleType::ROCK) {
                distanceMap[y][x] = 0.0f;
                continue;
            }

            float minDist = static_cast<float>(borderWidth + 1);
            bool nearestIsIsland = false;

            for (int dy = -borderWidth; dy <= borderWidth; ++dy) {
                for (int dx = -borderWidth; dx <= borderWidth; ++dx) {
                    int checkX = worldX + dx;
                    int checkY = worldY + dy;

                    // Convert to area coordinates
                    int areaX = checkX - areaStartX;
                    int areaY = checkY - areaStartY;

                    if (areaX < 0 || areaX >= areaSize || areaY < 0 || areaY >= areaSize) {
                        // Out of bounds = exterior
                        float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                        if (dist < minDist) {
                            minDist = dist;
                            nearestIsIsland = false;
                        }
                        continue;
                    }

                    int region = regionMap[areaY][areaX];
                    if (region == 1 || region == 2) { // Non-rock (exterior or island)
                        float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                        if (dist < minDist) {
                            minDist = dist;
                            nearestIsIsland = (region == 2);
                        }
                    }
                }
            }
            distanceMap[y][x] = minDist;
            adjacentToIsland[y][x] = nearestIsIsland;

        }
    }

    // Apply gradient and pattern
    float outerMult = config.rock.borderGradientOuterEdgeColorMultiplier;
    float innerMult = config.rock.borderGradientInnerEdgeColorMultiplier;
    bool isDotted = (config.rock.borderPattern == "dotted");
    int dotWidth = config.rock.borderPatternDottedDotWidth;
    int dotHeight = config.rock.borderPatternDottedDotHeight;
    int dotSpacing = config.rock.borderPatternDottedSpacing;

    for (int y = 0; y < WorldChunk::CHUNK_SIZE; ++y) {
        for (int x = 0; x < WorldChunk::CHUNK_SIZE; ++x) {
            int worldX = chunkWorldX + x;
            int worldY = chunkWorldY + y;

            if (world->getParticle(worldX, worldY) != ParticleType::ROCK) {
                continue;
            }

            float dist = distanceMap[y][x];

            if (dist > static_cast<float>(borderWidth)) {
                continue;
            }

            bool isIslandEdge = islandExcluded && adjacentToIsland[y][x];

            if (isIslandEdge) {
                // Island edge: only outer line (dist <= 1.5), 2x lighter than normal outer
                if (dist <= 1.5f) {
                    float islandMult = 1.0f - (1.0f - outerMult) * 0.5f; // Half the darkening
                    ParticleColor color = world->getColor(worldX, worldY);
                    world->setColor(worldX, worldY, {
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                            static_cast<float>(color.r) * islandMult))),
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                            static_cast<float>(color.g) * islandMult))),
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                            static_cast<float>(color.b) * islandMult)))
                    });
                }
                // Skip gradient/pattern for island edges
                continue;
            }

            // Exterior edge: full gradient
            float t = dist / static_cast<float>(borderWidth);
            t = std::max(0.0f, std::min(1.0f, t));
            float smoothT = t * t * (3.0f - 2.0f * t);
            float colorMult = outerMult + (innerMult - outerMult) * smoothT;

            bool applyPattern = true;
            if (isDotted) {
                int patternPeriodX = dotWidth + dotSpacing;
                int patternPeriodY = dotHeight + dotSpacing;
                int posInPatternX = ((worldX % patternPeriodX) + patternPeriodX) % patternPeriodX;
                int posInPatternY = ((worldY % patternPeriodY) + patternPeriodY) % patternPeriodY;
                applyPattern = (posInPatternX < dotWidth && posInPatternY < dotHeight);
            }

            if (applyPattern) {
                ParticleColor color = world->getColor(worldX, worldY);
                world->setColor(worldX, worldY, {
                    static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                        static_cast<float>(color.r) * colorMult))),
                    static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                        static_cast<float>(color.g) * colorMult))),
                    static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                        static_cast<float>(color.b) * colorMult)))
                });
            }
        }
    }
}

void Texturize::applyObsidianBorders(World* world, WorldChunk* chunk) {
    if (!world || !chunk) return;

    const Config& config = world->getConfig();
    if (!config.obsidian.borderEnabled) return;

    int chunkWorldX = chunk->getWorldX();
    int chunkWorldY = chunk->getWorldY();
    int borderWidth = config.obsidian.borderWidth;
    bool islandExcluded = config.obsidian.borderIslandExcluded;
    bool ignoreMoss = config.obsidian.borderIgnoreMoss;

    // Helper to check if a particle counts as "obsidian" for border purposes
    auto isObsidianLike = [ignoreMoss](ParticleType type) {
        if (type == ParticleType::OBSIDIAN) return true;
        if (ignoreMoss && type == ParticleType::MOSS) return true;
        return false;
    };

    int expandSize = 64;
    int areaSize = WorldChunk::CHUNK_SIZE + expandSize * 2;
    int areaStartX = chunkWorldX - expandSize;
    int areaStartY = chunkWorldY - expandSize;

    std::vector<std::vector<int>> regionMap(areaSize, std::vector<int>(areaSize, 0));

    for (int y = 0; y < areaSize; ++y) {
        for (int x = 0; x < areaSize; ++x) {
            int worldX = areaStartX + x;
            int worldY = areaStartY + y;

            if (!world->inWorldBounds(worldX, worldY)) {
                regionMap[y][x] = -1;
            } else if (!isObsidianLike(world->getParticle(worldX, worldY))) {
                regionMap[y][x] = -1;
            }
        }
    }

    std::vector<std::pair<int, int>> queue;
    for (int x = 0; x < areaSize; ++x) {
        if (regionMap[0][x] == -1) queue.push_back({x, 0});
        if (regionMap[areaSize-1][x] == -1) queue.push_back({x, areaSize-1});
    }
    for (int y = 1; y < areaSize - 1; ++y) {
        if (regionMap[y][0] == -1) queue.push_back({0, y});
        if (regionMap[y][areaSize-1] == -1) queue.push_back({areaSize-1, y});
    }

    while (!queue.empty()) {
        auto [x, y] = queue.back();
        queue.pop_back();

        if (x < 0 || x >= areaSize || y < 0 || y >= areaSize) continue;
        if (regionMap[y][x] != -1) continue;

        regionMap[y][x] = 1;

        queue.push_back({x-1, y});
        queue.push_back({x+1, y});
        queue.push_back({x, y-1});
        queue.push_back({x, y+1});
    }

    for (int y = 0; y < areaSize; ++y) {
        for (int x = 0; x < areaSize; ++x) {
            if (regionMap[y][x] == -1) {
                regionMap[y][x] = 2;
            }
        }
    }

    std::vector<std::vector<float>> distanceMap(WorldChunk::CHUNK_SIZE,
        std::vector<float>(WorldChunk::CHUNK_SIZE, static_cast<float>(borderWidth + 1)));
    std::vector<std::vector<bool>> adjacentToIsland(WorldChunk::CHUNK_SIZE,
        std::vector<bool>(WorldChunk::CHUNK_SIZE, false));

    for (int y = 0; y < WorldChunk::CHUNK_SIZE; ++y) {
        for (int x = 0; x < WorldChunk::CHUNK_SIZE; ++x) {
            int worldX = chunkWorldX + x;
            int worldY = chunkWorldY + y;

            if (world->getParticle(worldX, worldY) != ParticleType::OBSIDIAN) {
                distanceMap[y][x] = 0.0f;
                continue;
            }

            float minDist = static_cast<float>(borderWidth + 1);
            bool nearestIsIsland = false;

            for (int dy = -borderWidth; dy <= borderWidth; ++dy) {
                for (int dx = -borderWidth; dx <= borderWidth; ++dx) {
                    int checkX = worldX + dx;
                    int checkY = worldY + dy;

                    int areaX = checkX - areaStartX;
                    int areaY = checkY - areaStartY;

                    if (areaX < 0 || areaX >= areaSize || areaY < 0 || areaY >= areaSize) {
                        float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                        if (dist < minDist) {
                            minDist = dist;
                            nearestIsIsland = false;
                        }
                        continue;
                    }

                    int region = regionMap[areaY][areaX];
                    if (region == 1 || region == 2) {
                        float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                        if (dist < minDist) {
                            minDist = dist;
                            nearestIsIsland = (region == 2);
                        }
                    }
                }
            }
            distanceMap[y][x] = minDist;
            adjacentToIsland[y][x] = nearestIsIsland;
        }
    }

    float outerMult = config.obsidian.borderGradientOuterEdgeColorMultiplier;
    float innerMult = config.obsidian.borderGradientInnerEdgeColorMultiplier;
    bool isDotted = (config.obsidian.borderPattern == "dotted");
    int dotWidth = config.obsidian.borderPatternDottedDotWidth;
    int dotHeight = config.obsidian.borderPatternDottedDotHeight;
    int dotSpacing = config.obsidian.borderPatternDottedSpacing;

    for (int y = 0; y < WorldChunk::CHUNK_SIZE; ++y) {
        for (int x = 0; x < WorldChunk::CHUNK_SIZE; ++x) {
            int worldX = chunkWorldX + x;
            int worldY = chunkWorldY + y;

            if (world->getParticle(worldX, worldY) != ParticleType::OBSIDIAN) {
                continue;
            }

            float dist = distanceMap[y][x];

            if (dist > static_cast<float>(borderWidth)) {
                continue;
            }

            bool isIslandEdge = islandExcluded && adjacentToIsland[y][x];

            if (isIslandEdge) {
                if (dist <= 1.5f) {
                    float islandMult = 1.0f - (1.0f - outerMult) * 0.5f;
                    ParticleColor color = world->getColor(worldX, worldY);
                    world->setColor(worldX, worldY, {
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                            static_cast<float>(color.r) * islandMult))),
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                            static_cast<float>(color.g) * islandMult))),
                        static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                            static_cast<float>(color.b) * islandMult)))
                    });
                }
                continue;
            }

            float t = dist / static_cast<float>(borderWidth);
            t = std::max(0.0f, std::min(1.0f, t));
            float smoothT = t * t * (3.0f - 2.0f * t);
            float colorMult = outerMult + (innerMult - outerMult) * smoothT;

            bool applyPattern = true;
            if (isDotted) {
                int patternPeriodX = dotWidth + dotSpacing;
                int patternPeriodY = dotHeight + dotSpacing;
                int posInPatternX = ((worldX % patternPeriodX) + patternPeriodX) % patternPeriodX;
                int posInPatternY = ((worldY % patternPeriodY) + patternPeriodY) % patternPeriodY;
                applyPattern = (posInPatternX < dotWidth && posInPatternY < dotHeight);
            }

            if (applyPattern) {
                ParticleColor color = world->getColor(worldX, worldY);
                world->setColor(worldX, worldY, {
                    static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                        static_cast<float>(color.r) * colorMult))),
                    static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                        static_cast<float>(color.g) * colorMult))),
                    static_cast<unsigned char>(std::max(0.0f, std::min(255.0f,
                        static_cast<float>(color.b) * colorMult)))
                });
            }
        }
    }
}