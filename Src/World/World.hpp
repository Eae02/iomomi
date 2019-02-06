#pragma once

#include <cstddef>

#include "Dir.hpp"
#include "../Graphics/Vertex.hpp"

struct WallVertex;

struct ClippingArgs
{
	glm::vec3 aabbMin;
	glm::vec3 aabbMax;
	glm::vec3 move;
	glm::vec3 colPlaneNormal;
	float clipDist;
};

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
	
	bool Load(std::istream& stream);
	
	void Save(std::ostream& outStream) const;
	
	void SetIsAir(const glm::ivec3& pos, bool isAir);
	
	bool IsAir(const glm::ivec3& pos) const;
	
	void SetTexture(const glm::ivec3& pos, Dir side, uint8_t textureLayer);
	
	uint8_t GetTexture(const glm::ivec3& pos, Dir side) const;
	
	void PrepareForDraw(class ObjectRenderer& objectRenderer, bool isEditor);
	
	void Draw();
	void DrawEditor();
	
	void CalcClipping(ClippingArgs& args) const;
	
	bool IsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir) const;
	void SetIsGravityCorner(const glm::ivec3& cornerPos, Dir cornerDir, bool value);
	bool IsCorner(const glm::ivec3& cornerPos, Dir cornerDir) const;
	
	const GravityCorner* FindGravityCorner(const ClippingArgs& args, Dir currentDown) const;
	
	PickWallResult PickWall(const eg::Ray& ray) const;
	
private:
	static constexpr uint32_t REGION_SIZE = 16;
	
	inline static std::tuple<glm::ivec3, glm::ivec3> DecomposeGlobalCoordinate(const glm::ivec3& globalC);
	
	/**
	 * Each voxel is represented by one 64-bit integer where the low 48 bits store which texture is used
	 * for each side (with 8 bits per side).
	 * Bits 48-59 store if the sides of this voxel are gravity corners.
	 * The 60th bit is a 1 if the voxel is air, or 0 otherwise.
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
	
	const Region* GetRegion(const glm::ivec3& coordinate) const;
	Region* GetRegion(const glm::ivec3& coordinate, bool maybeCreate);
	
	void BuildRegionMesh(glm::ivec3 coordinate, RegionData& region);
	void BuildRegionBorderMesh(glm::ivec3 coordinate, RegionData& region);
	
	glm::ivec4 GetGravityCornerVoxelPos(glm::ivec3 cornerPos, Dir cornerDir) const;
	
	std::vector<Region> m_regions;
	
	bool m_anyOutOfDate = true;
	bool m_canDraw = false;
	
	eg::Buffer m_voxelVertexBuffer;
	eg::Buffer m_voxelIndexBuffer;
	size_t m_voxelVertexBufferCapacity = 0;
	size_t m_voxelIndexBufferCapacity = 0;
	
	eg::Buffer m_borderVertexBuffer;
	size_t m_borderVertexBufferCapacity = 0;
	uint32_t m_numBorderVertices;
	
	std::vector<GravityCorner> m_gravityCorners;
};
