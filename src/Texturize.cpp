#include "Texturize.h"
#include "World.h"
#include "WorldChunk.h"
#include "Config.h"
#include <cstdlib>
#include <algorithm>

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
