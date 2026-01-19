#pragma once
#include "SandSimulator.h"  // For ParticleType, ParticleColor, ParticleVelocity
#include "WorldChunk.h"
#include "SceneObject.h"
#include "Config.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <functional>
#include <cmath>
#include <algorithm>

// Camera for viewport tracking with smooth follow
struct Camera {
    float x, y;           // World position (top-left of viewport)
    int viewportWidth;    // Viewport size in pixels
    int viewportHeight;
    float moveSpeed;      // Pixels per second (for manual movement)

    // Deadzone - player can move within this box before camera follows
    float deadzoneWidth;   // Width of deadzone box (centered on screen)
    float deadzoneHeight;  // Height of deadzone box

    // Smoothing
    float smoothSpeed;     // How fast camera catches up (higher = faster, 1-10 typical)

    // Look-ahead - camera shifts ahead in movement direction
    float lookAheadX;      // Current look-ahead offset X
    float lookAheadY;      // Current look-ahead offset Y
    float lookAheadMaxX;   // Maximum look-ahead distance X
    float lookAheadMaxY;   // Maximum look-ahead distance Y
    float lookAheadSpeed;  // How fast look-ahead adjusts

    // Target tracking
    float targetX, targetY; // Where camera wants to be

    Camera() : x(0), y(0), viewportWidth(12), viewportHeight(12), moveSpeed(25.0f),
               deadzoneWidth(200.0f), deadzoneHeight(120.0f),
               smoothSpeed(4.0f),
               lookAheadX(0), lookAheadY(0),
               lookAheadMaxX(80.0f), lookAheadMaxY(50.0f),
               lookAheadSpeed(2.0f),
               targetX(0), targetY(0) {}

    // Update camera to follow a target position with movement direction
    void update(float playerX, float playerY, float playerWidth, float playerHeight,
                float moveDirX, float moveDirY,
                float worldWidth, float worldHeight, float deltaTime) {

        // Calculate player center
        float playerCenterX = playerX + playerWidth / 2.0f;
        float playerCenterY = playerY + playerHeight / 2.0f;

        // Calculate current camera center
        float camCenterX = x + viewportWidth / 2.0f;
        float camCenterY = y + viewportHeight / 2.0f;

        // Update look-ahead based on movement direction (smooth transition)
        float targetLookAheadX = moveDirX * lookAheadMaxX;
        float targetLookAheadY = moveDirY * lookAheadMaxY;
        lookAheadX += (targetLookAheadX - lookAheadX) * lookAheadSpeed * deltaTime;
        lookAheadY += (targetLookAheadY - lookAheadY) * lookAheadSpeed * deltaTime;

        // Check if player is outside deadzone (relative to camera center minus look-ahead)
        float effectiveCenterX = camCenterX - lookAheadX;
        float effectiveCenterY = camCenterY - lookAheadY;

        float halfDeadzoneW = deadzoneWidth / 2.0f;
        float halfDeadzoneH = deadzoneHeight / 2.0f;

        // Calculate where player is relative to deadzone
        float leftEdge = effectiveCenterX - halfDeadzoneW;
        float rightEdge = effectiveCenterX + halfDeadzoneW;
        float topEdge = effectiveCenterY - halfDeadzoneH;
        float bottomEdge = effectiveCenterY + halfDeadzoneH;

        // Push target if player exits deadzone
        if (playerCenterX < leftEdge) {
            targetX = playerCenterX + halfDeadzoneW - viewportWidth / 2.0f + lookAheadX;
        } else if (playerCenterX > rightEdge) {
            targetX = playerCenterX - halfDeadzoneW - viewportWidth / 2.0f + lookAheadX;
        } else {
            // Player inside deadzone horizontally - only apply look-ahead drift
            targetX = x + lookAheadX * deltaTime * 0.5f;
        }

        if (playerCenterY < topEdge) {
            targetY = playerCenterY + halfDeadzoneH - viewportHeight / 2.0f + lookAheadY;
        } else if (playerCenterY > bottomEdge) {
            targetY = playerCenterY - halfDeadzoneH - viewportHeight / 2.0f + lookAheadY;
        } else {
            // Player inside deadzone vertically - only apply look-ahead drift
            targetY = y + lookAheadY * deltaTime * 0.5f;
        }

        // Clamp target to world bounds (camera can't show outside world)
        float maxCamX = worldWidth - viewportWidth;
        float maxCamY = worldHeight - viewportHeight;
        targetX = std::max(0.0f, std::min(targetX, maxCamX));
        targetY = std::max(0.0f, std::min(targetY, maxCamY));

        // Smoothly interpolate camera position towards target
        float lerpFactor = 1.0f - std::exp(-smoothSpeed * deltaTime);
        x += (targetX - x) * lerpFactor;
        y += (targetY - y) * lerpFactor;

        // Final clamp to ensure camera never goes out of bounds
        x = std::max(0.0f, std::min(x, maxCamX));
        y = std::max(0.0f, std::min(y, maxCamY));
    }

    // Instant center on position (for initialization)
    void centerOn(float posX, float posY, float worldWidth, float worldHeight) {
        x = posX - viewportWidth / 2.0f;
        y = posY - viewportHeight / 2.0f;

        // Clamp to world bounds
        float maxCamX = worldWidth - viewportWidth;
        float maxCamY = worldHeight - viewportHeight;
        x = std::max(0.0f, std::min(x, maxCamX));
        y = std::max(0.0f, std::min(y, maxCamY));

        targetX = x;
        targetY = y;
    }
};

// Hash function for chunk coordinates
struct ChunkKey {
    int x, y;

    bool operator==(const ChunkKey& other) const {
        return x == other.x && y == other.y;
    }
};

struct ChunkKeyHash {
    std::size_t operator()(const ChunkKey& key) const {
        return std::hash<int>()(key.x) ^ (std::hash<int>()(key.y) << 16);
    }
};

struct ParticleChunk {
    bool isAwake = true;
    int stableFrames = 0;
};

// Enemy spawn marker types (detected from level image colors)
enum class SpawnMarkerType {
    LITTLE_PURPLE_JUMPER  // #450981 - RGB(69, 9, 129)
};

struct EnemySpawnPoint {
    int worldX;
    int worldY;
    SpawnMarkerType type;
    bool spawned;  // Has an enemy been created here?
};


class World {
public:
    // World size in chunks (70x70 = 35,840 x 35,840 pixels)
    static constexpr int WORLD_CHUNKS_X = 70;
    static constexpr int WORLD_CHUNKS_Y = 70;
    static constexpr int WORLD_WIDTH = WORLD_CHUNKS_X * WorldChunk::CHUNK_SIZE;   // 35,840
    static constexpr int WORLD_HEIGHT = WORLD_CHUNKS_Y * WorldChunk::CHUNK_SIZE;  // 35,840

    // Particle chunk properties
    static constexpr int PARTICLE_CHUNK_WIDTH = 10;
    static constexpr int PARTICLE_CHUNK_HEIGHT = 10;
    static constexpr int P_CHUNKS_X = (WORLD_WIDTH + PARTICLE_CHUNK_WIDTH - 1) / PARTICLE_CHUNK_WIDTH;
    static constexpr int P_CHUNKS_Y = (WORLD_HEIGHT + PARTICLE_CHUNK_HEIGHT - 1) / PARTICLE_CHUNK_HEIGHT;
    static constexpr int P_CHUNK_FRAMES_UNTIL_SLEEP = 15;


    // How many chunks around the camera to keep loaded/active
    static constexpr int LOAD_RADIUS = 3;      // Load chunks within this radius

    World(const Config& config);
    ~World();  // Need destructor to free scene image

    // Camera control
    Camera& getCamera() { return camera; }
    const Camera& getCamera() const { return camera; }
    void moveCamera(float dx, float dy, float deltaTime);

    // Particle access (world coordinates)
    ParticleType getParticle(int worldX, int worldY) const;
    void setParticle(int worldX, int worldY, ParticleType type);
    ParticleColor getColor(int worldX, int worldY) const;
    void setColor(int worldX, int worldY, ParticleColor color);

    bool isOccupied(int worldX, int worldY) const;
    void spawnParticleAt(int worldX, int worldY, ParticleType type);

    float getWetness(int worldX, int worldY) const;
    void setWetness(int worldX, int worldY, float wetness);

    // Simulation
    void update(float deltaTime);

    // Chunk management
    WorldChunk* getChunk(int chunkX, int chunkY);
    const WorldChunk* getChunk(int chunkX, int chunkY) const;
    WorldChunk* getChunkAtWorldPos(int worldX, int worldY);
    const WorldChunk* getChunkAtWorldPos(int worldX, int worldY) const;

    void loadChunksAroundCamera();
    void unloadDistantChunks();

    // World coordinate helpers
    static void worldToChunk(int worldX, int worldY, int& chunkX, int& chunkY);
    static void worldToLocal(int worldX, int worldY, int& localX, int& localY);
    static void chunkToWorld(int chunkX, int chunkY, int& worldX, int& worldY);
    static void worldToParticleChunk(int worldX, int worldY, int& pcX, int& pcY);


    bool inWorldBounds(int worldX, int worldY) const;

    // Scene loading - lazy load from image as chunks come into view
    bool setSceneImage(const std::string& filepath);
    bool loadSceneFromBMP(const std::string& filepath, int worldOffsetX = 0, int worldOffsetY = 0);

    // Rendering helpers
    int getViewportWidth() const { return camera.viewportWidth; }
    int getViewportHeight() const { return camera.viewportHeight; }
    void setViewportSize(int w, int h) { camera.viewportWidth = w; camera.viewportHeight = h; }

    // Get visible region in world coordinates
    void getVisibleRegion(int& startX, int& startY, int& endX, int& endY) const;

    // Particle counts
    int getParticleCount(ParticleType type) const;

    // Config access
    const Config& getConfig() const { return config; }

    // Scene objects
    void addSceneObject(std::shared_ptr<SceneObject> obj);
    void removeSceneObject(SceneObject* obj);
    const std::vector<std::shared_ptr<SceneObject>>& getSceneObjects() const { return sceneObjects; }

    const std::unordered_map<ChunkKey, std::unique_ptr<WorldChunk>, ChunkKeyHash>& getChunks() const { return chunks; }

    const std::vector<ParticleChunk>& getParticleChunks() const { return particleChunks; }

    // Enemy spawn points detected from level image markers
    std::vector<EnemySpawnPoint>& getEnemySpawnPoints() { return enemySpawnPoints; }
    const std::vector<EnemySpawnPoint>& getEnemySpawnPoints() const { return enemySpawnPoints; }

    // Check if a world position is blocked by a scene object
    bool isBlockedBySceneObject(int worldX, int worldY) const;

    // Explode particles in a radius - unsettles them and applies outward velocity
    void explodeAt(int worldX, int worldY, int radius, float force);

    // Get mass of a particle type (for explosion scaling)
    float getParticleMass(ParticleType type) const;

    // Collision detection for player/entities
    bool isSolidParticle(ParticleType type) const;
    bool checkCapsuleCollision(float centerX, float centerY, float radius, float height, float& collisionY) const;

private:
    Config config;
    Camera camera;

    // Loaded chunks (sparse storage - only chunks that exist are stored)
    std::unordered_map<ChunkKey, std::unique_ptr<WorldChunk>, ChunkKeyHash> chunks;

    // Particle chunk management for sleeping
    std::vector<ParticleChunk> particleChunks;
    std::vector<bool> particleChunkActivity;

    // Scene objects (non-particle entities)
    std::vector<std::shared_ptr<SceneObject>> sceneObjects;

    // Enemy spawn points detected from marker colors in level image
    std::vector<EnemySpawnPoint> enemySpawnPoints;

    // Simulation helpers
    void updateParticle(int worldX, int worldY);

    // Particle physics (simplified versions that work across chunks)
    void updateSandParticle(int worldX, int worldY);
    void updateWaterParticle(int worldX, int worldY);
    void updateLavaParticle(int worldX, int worldY);
    void updateSteamParticle(int worldX, int worldY);
    void updateFireParticle(int worldX, int worldY);
    void updateIceParticle(int worldX, int worldY);
    void updateMossParticle(int worldX, int worldY);
    void updateWetnessForParticle(int x, int y);

    // Movement helpers
    bool canMoveTo(int worldX, int worldY) const;
    void moveParticle(int fromX, int fromY, int toX, int toY);
    void swapParticles(int x1, int y1, int x2, int y2);
    void markSettled(int worldX, int worldY, bool settled);

    // Color generation
    ParticleColor generateRandomColor(int baseR, int baseG, int baseB, int variation);

    // Sleep system
    static constexpr int FRAMES_UNTIL_SLEEP = 30;
    void wakeChunkAtWorldPos(int worldX, int worldY);

    // Scene image for lazy loading (stored in memory)
    unsigned char* sceneImageData = nullptr;
    int sceneImageWidth = 0;
    int sceneImageHeight = 0;
    std::unordered_map<ChunkKey, bool, ChunkKeyHash> chunksPopulatedFromScene;

    void populateChunkFromScene(WorldChunk* chunk);
    void procedurallyGenerateMoss(WorldChunk* chunk);
    float getMaxSaturation(ParticleType type) const;
};
