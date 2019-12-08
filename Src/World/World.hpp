#pragma once

#include <cstddef>

#include "Dir.hpp"
#include "BulletPhysics.hpp"
#include "WorldUpdateArgs.hpp"
#include "Door.hpp"
#include "Entities/EntityManager.hpp"
#include "../Graphics/Vertex.hpp"
#include "../Graphics/PlanarReflectionsManager.hpp"

struct WallVertex;

struct WallRayIntersectResult
{
	bool intersected;
	glm::ivec3 voxelPosition;
	glm::vec3 intersectPosition;
	Dir normalDir;
};

struct RayIntersectResult
{
	bool intersected;
	Ent* entity;
	float distance;
};

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

struct WallSideMaterial
{
	int texture;
	bool enableReflections;
};

class World
{
public:
	World() = default;
	
	//Move breaks bullet
	World(World&&) = delete;
	World(const World&) = delete;
	World& operator=(World&&) = delete;
	World& operator=(const World&) = delete;
	
	static std::unique_ptr<World> Load(std::istream& stream, bool isEditor);
	
	void Save(std::ostream& outStream) const;
	
	void SetIsAir(const glm::ivec3& pos, bool isAir);
	
	bool IsAir(const glm::ivec3& pos) const;
	
	void SetMaterialSafe(const glm::ivec3& pos, Dir side, WallSideMaterial material)
	{
		if (IsAir(pos))
		{
			SetMaterial(pos, side, material);
		}
	}
	
	void SetMaterial(const glm::ivec3& pos, Dir side, WallSideMaterial material);
	
	WallSideMaterial GetMaterial(const glm::ivec3& pos, Dir side) const;
	
	bool HasCollision(const glm::ivec3& pos, Dir side) const;
	
	void Update(const WorldUpdateArgs& args);
	
	void PrepareForDraw(struct PrepareDrawArgs& args);
	
	void Draw();
	void DrawEditor();
	void DrawPlanarReflections(const eg::Plane& plane);
	void DrawPointLightShadows(const struct PointLightShadowRenderArgs& renderArgs);
	
	bool IsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir) const;
	void SetIsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir, bool value);
	bool IsCorner(const glm::ivec3& cornerPos, Dir cornerDir) const;
	
	const GravityCorner* FindGravityCorner(const eg::AABB& aabb, glm::vec3 move, Dir currentDown) const;
	
	WallRayIntersectResult RayIntersectWall(const eg::Ray& ray) const;
	
	RayIntersectResult RayIntersect(const eg::Ray& ray) const;
	
	void InitializeBulletPhysics();
	
	//Adds the rigid body belonging to the given entity to the world.
	//This needs to be called for all entities with an ECRigidBody created after world loading.
	void InitRigidBodyEntity(Ent& entity);
	
	btDiscreteDynamicsWorld* PhysicsWorld()
	{
		return m_bulletWorld.get();
	}
	
	EntityManager entManager;
	
	const glm::ivec3& GetBoundsMin() const
	{
		return m_boundsMin;
	}
	
	const glm::ivec3& GetBoundsMax() const
	{
		return m_boundsMax;
	}
	
	bool playerHasGravityGun = false;
	
private:
	static constexpr uint32_t REGION_SIZE = 16;
	
	inline static std::tuple<glm::ivec3, glm::ivec3> DecomposeGlobalCoordinate(const glm::ivec3& globalC);
	
	struct VoxelReflectionPlane
	{
		Dir normalDir;
		int distance;
		
		bool operator==(const VoxelReflectionPlane& other) const
		{
			return normalDir == other.normalDir && distance == other.distance;
		}
	};
	
	/**
	 * Each voxel is represented by one 64-bit integer where the low 48 bits store which texture is used
	 * for each side and if reflection is enabled (7 bits for texture and 1 bit for reflection).
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
		std::vector<uint16_t> indicesReflective;
		std::vector<WallBorderVertex> borderVertices;
		std::vector<GravityCorner> gravityCorners;
		std::vector<VoxelReflectionPlane> reflectionPlanes;
		eg::CollisionMesh collisionMesh;
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
	
	void BuildCollisionMesh();
	
	void PrepareRegionMeshes(bool isEditor);
	
	const Region* GetRegion(const glm::ivec3& coordinate) const;
	Region* GetRegion(const glm::ivec3& coordinate, bool maybeCreate);
	
	void BuildRegionMesh(glm::ivec3 coordinate, RegionData& region, bool includeNoDraw);
	void BuildRegionBorderMesh(glm::ivec3 coordinate, RegionData& region);
	
	glm::ivec4 GetGravityCornerVoxelPos(glm::ivec3 cornerPos, Dir cornerDir) const;
	
	std::vector<Region> m_regions;
	
	std::unique_ptr<btDbvtBroadphase> m_bulletBroadphase;
	std::shared_ptr<btDiscreteDynamicsWorld> m_bulletWorld;
	
	std::vector<Door> m_doors;
	
	bool m_anyOutOfDate = true;
	bool m_canDraw = false;
	
	eg::Buffer m_voxelVertexBuffer;
	eg::Buffer m_voxelIndexBuffer;
	size_t m_voxelVertexBufferCapacity = 0;
	size_t m_voxelIndexBufferCapacity = 0;
	
	eg::Buffer m_borderVertexBuffer;
	size_t m_borderVertexBufferCapacity = 0;
	uint32_t m_numBorderVertices;
	
	float m_accumulatedPhysicsTime;
	
	glm::ivec3 m_boundsMin;
	glm::ivec3 m_boundsMax;
	
	std::vector<ReflectionPlane> m_wallReflectionPlanes;
	
	std::unique_ptr<btTriangleMesh> m_wallsBulletMesh;
	std::unique_ptr<btDefaultMotionState> m_wallsMotionState;
	std::unique_ptr<btBvhTriangleMeshShape> m_wallsBulletShape;
	std::unique_ptr<btRigidBody> m_wallsRigidBody;
};
