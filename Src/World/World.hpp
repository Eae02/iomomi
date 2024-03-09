#pragma once

#include <bitset>
#include <cstddef>

#include "../Graphics/GraphicsCommon.hpp"
#include "../Graphics/Vertex.hpp"
#include "Collision.hpp"
#include "Dir.hpp"
#include "Door.hpp"
#include "Entities/EntityManager.hpp"
#include "PhysicsEngine.hpp"
#include "VoxelBuffer.hpp"
#include "WorldUpdateArgs.hpp"

struct WallVertex;

struct GravityCorner
{
	Dir down1;
	Dir down2;
	glm::vec3 position;
	bool isEnd[2];

	/**
	 * Creates a rotation matrix for rotating from local space to world space.
	 * In local space, +X is in the opposite direction of down1, +Y is in the opposite direction of down2,
	 * and +Z points along the corner.
	 */
	glm::mat3 MakeRotationMatrix() const;
};

enum class OptionalControlHintType
{
	Movement,
	FlipGravity,
	PickUpCube,
	CannotChangeGravityWhenCarrying,
	CannotExitWithWrongGravity,
};

constexpr int NUM_OPTIONAL_CONTROL_HINTS = 5;

class World
{
public:
	World();

	static std::unique_ptr<World> Load(std::istream& stream, bool isEditor);

	void Save(std::ostream& outStream) const;

	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt);

	void Update(const WorldUpdateArgs& args);
	void UpdateAfterPhysics(const WorldUpdateArgs& args);

	void PrepareForDraw(struct PrepareDrawArgs& args);

	void Draw();
	void DrawEditor(bool drawGrid);
	void DrawPointLightShadows(const struct PointLightShadowDrawArgs& renderArgs);

	bool HasCollision(const glm::ivec3& pos, Dir side) const;

	const GravityCorner* FindGravityCorner(const eg::AABB& aabb, glm::vec3 move, Dir currentDown) const;

	float MaxDistanceToWallVertex(const glm::vec3& pos) const;

	EntityManager entManager;

	bool IsLatestVersion() const { return m_isLatestVersion; }

	bool playerHasGravityGun = true;
	std::string title;
	uint32_t extraWaterParticles = 0;
	uint32_t waterPresimIterations = 100;

	glm::vec3 thumbnailCameraPos;
	glm::vec3 thumbnailCameraDir;

	bool showControlHint[NUM_OPTIONAL_CONTROL_HINTS] = {};

	eg::ColorSRGB ssrFallbackColor;
	float ssrIntensity = 1;

	VoxelBuffer voxels;

private:
	std::vector<GravityCorner> m_gravityCorners;

	void PrepareMeshes(bool isEditor);
	eg::CollisionMesh BuildMesh(std::vector<WallVertex>& vertices, std::vector<uint32_t>& indices, bool includeNoDraw)
		const;
	void BuildBorderMesh(std::vector<WallBorderVertex>* borderVertices, std::vector<GravityCorner>& gravityCorners)
		const;

	std::vector<glm::ivec3> cubeSpawnerPositions2;
	std::vector<Door> m_doors;

	bool m_canDraw = false;
	bool m_isLatestVersion = false;

	eg::CollisionMesh m_collisionMesh;
	PhysicsObject m_physicsObject;

	ResizableBuffer m_voxelVertexBuffer;
	ResizableBuffer m_voxelIndexBuffer;
	uint32_t m_numVoxelIndices = 0;

	ResizableBuffer m_borderVertexBuffer;
	uint32_t m_numBorderVertices = 0;
};
