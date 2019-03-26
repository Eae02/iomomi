#pragma once

#include <cstddef>

#include "Dir.hpp"
#include "Clipping.hpp"
#include "BulletPhysics.hpp"
#include "Entities/Entity.hpp"
#include "Entities/SpotLightEntity.hpp"
#include "Entities/PointLightEntity.hpp"
#include "Entities/GravitySwitchEntity.hpp"
#include "../Graphics/Vertex.hpp"

struct WallVertex;

struct PickWallResult
{
	bool intersected;
	glm::ivec3 voxelPosition;
	glm::vec3 intersectPosition;
	Dir normalDir;
};

struct GravityCorner
{
	Dir down1;
	Dir down2;
	glm::vec3 position;
	
	/**
	 * Creates a rotation matrix for rotating from local space to world space.
	 * In local space, +X is in the opposite direction of down1, +Y is in the opposite direction of down2,
	 * and +Z points along the corner.
	 */
	glm::mat3 MakeRotationMatrix() const;
};

class World
{
public:
	World();
	
	//Move breaks bullet
	World(World&&) = delete;
	World(const World&) = delete;
	World& operator=(World&&) = delete;
	World& operator=(const World&) = delete;
	
	static std::unique_ptr<World> Load(std::istream& stream);
	
	void Save(std::ostream& outStream) const;
	
	void SetIsAir(const glm::ivec3& pos, bool isAir);
	
	bool IsAir(const glm::ivec3& pos) const;
	
	void SetTextureSafe(const glm::ivec3& pos, Dir side, uint8_t textureLayer)
	{
		if (IsAir(pos))
		{
			SetTexture(pos, side, textureLayer);
		}
	}
	
	void SetTexture(const glm::ivec3& pos, Dir side, uint8_t textureLayer);
	
	uint8_t GetTexture(const glm::ivec3& pos, Dir side) const;
	
	void AddEntity(std::shared_ptr<Entity> entity);
	
	void Update(const Entity::UpdateArgs& args);
	
	void PrepareForDraw(struct PrepareDrawArgs& args);
	
	void DespawnEntity(const Entity* entity);
	
	void Draw();
	void DrawEditor();
	void DrawPointLightShadows(const struct PointLightShadowRenderArgs& renderArgs);
	
	bool IsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir) const;
	void SetIsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir, bool value);
	bool IsCorner(const glm::ivec3& cornerPos, Dir cornerDir) const;
	
	const GravityCorner* FindGravityCorner(const ClippingArgs& args, Dir currentDown) const;
	
	std::shared_ptr<GravitySwitchEntity> FindGravitySwitch(const eg::AABB& aabb, Dir currentDown) const;
	
	PickWallResult PickWall(const eg::Ray& ray) const;
	
	void InitializeBulletPhysics();
	
	const std::vector<std::shared_ptr<Entity>>& Entities() const
	{
		return m_entities;
	}
	
	const std::vector<std::weak_ptr<Entity::ICollidable>>& CollidableEntities() const
	{
		return m_collidables;
	}
	
private:
	static constexpr uint32_t REGION_SIZE = 16;
	
	inline static std::tuple<glm::ivec3, glm::ivec3> DecomposeGlobalCoordinate(const glm::ivec3& globalC);
	
	/**
	 * Each voxel is represented by one 64-bit integer where the low 48 bits store which texture is used
	 * for each side (with 8 bits per side).
	 * Bits 48-59 store if the sides of this voxel are gravity corners.
	 * The 60th bit is a 1 if the voxel is air, or 0 otherwise.
	 * Texture information is stored in the air voxel that the solid voxel faces into.
	 */
	struct RegionData
	{
		uint32_t numAirVoxels = 0;
		uint32_t firstVertex;
		uint32_t firstIndex;
		uint64_t voxels[REGION_SIZE][REGION_SIZE][REGION_SIZE] = { };
		std::vector<WallVertex> vertices;
		std::vector<uint16_t> indices;
		std::vector<WallBorderVertex> borderVertices;
		std::vector<GravityCorner> gravityCorners;
	};
	
	struct Region
	{
		glm::ivec3 coordinate;
		bool voxelsOutOfDate;
		bool gravityCornersOutOfDate;
		bool canDraw;
		std::unique_ptr<RegionData> data;
		
		bool operator<(const glm::ivec3& c) const
		{
			return std::tie(coordinate.x, coordinate.y, coordinate.z) < std::tie(c.x, c.y, c.z);
		}
		
		bool operator<(const Region& other) const
		{
			return operator<(other.coordinate);
		}
	};
	
	void PrepareRegionMeshes(bool isEditor);
	
	const Region* GetRegion(const glm::ivec3& coordinate) const;
	Region* GetRegion(const glm::ivec3& coordinate, bool maybeCreate);
	
	void BuildRegionMesh(glm::ivec3 coordinate, RegionData& region, bool includeNoDraw);
	void BuildRegionBorderMesh(glm::ivec3 coordinate, RegionData& region);
	
	glm::ivec4 GetGravityCornerVoxelPos(glm::ivec3 cornerPos, Dir cornerDir) const;
	
	std::vector<Region> m_regions;
	
	std::vector<std::shared_ptr<Entity>> m_entities;
	std::vector<std::weak_ptr<SpotLightEntity>> m_spotLights;
	std::vector<std::weak_ptr<PointLightEntity>> m_pointLights;
	std::vector<std::weak_ptr<GravitySwitchEntity>> m_gravitySwitchEntities;
	std::vector<std::weak_ptr<Entity::IUpdatable>> m_updatables;
	std::vector<std::weak_ptr<Entity::IDrawable>> m_drawables;
	std::vector<std::weak_ptr<Entity::ICollidable>> m_collidables;
	
	bool m_anyOutOfDate = true;
	bool m_canDraw = false;
	
	eg::Buffer m_voxelVertexBuffer;
	eg::Buffer m_voxelIndexBuffer;
	size_t m_voxelVertexBufferCapacity = 0;
	size_t m_voxelIndexBufferCapacity = 0;
	
	eg::Buffer m_borderVertexBuffer;
	size_t m_borderVertexBufferCapacity = 0;
	uint32_t m_numBorderVertices;
	
	std::unique_ptr<btTriangleMesh> m_wallsBulletMesh;
	std::unique_ptr<btDefaultMotionState> m_wallsMotionState;
	std::unique_ptr<btBvhTriangleMeshShape> m_wallsBulletShape;
	std::unique_ptr<btRigidBody> m_wallsRigidBody;
	
	std::unique_ptr<btDbvtBroadphase> m_bulletBroadphase;
	std::unique_ptr<btDiscreteDynamicsWorld> m_bulletWorld;
};
