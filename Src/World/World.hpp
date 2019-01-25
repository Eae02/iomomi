#pragma once

#include <cstddef>

#include "Dir.hpp"

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
	float length;
	
	/**
	 * Creates a rotation matrix for rotating from local space to world space.
	 * In local space, +X is in the opposite direction of down1, +Y is in the opposite direction of down2,
	 * and +Z points along the corner.
	 */
	glm::mat3 MakeRotationMatrix() const;
	
	GravityCorner() = default;
	GravityCorner(Dir _down1, Dir _down2, const glm::vec3& _position, float _length)
		: down1(_down1), down2(_down2), position(_position), length(_length) { }
};

class World
{
public:
	World();
	
	bool Load(std::istream& stream);
	
	void Save(std::ostream& outStream) const;
	
	void AddGravityCorner(const GravityCorner& corner)
	{
		m_gravityCorners.push_back(corner);
	}
	
	void SetIsAir(const glm::ivec3& pos, bool isAir);
	
	bool IsAir(const glm::ivec3& pos) const;
	
	void SetTexture(const glm::ivec3& pos, Dir side, uint8_t textureLayer);
	
	uint8_t GetTexture(const glm::ivec3& pos, Dir side) const;
	
	void PrepareForDraw(class ObjectRenderer& objectRenderer);
	
	void Draw(const class RenderSettings& renderSettings);
	
	void CalcClipping(ClippingArgs& args) const;
	
	const GravityCorner* FindGravityCorner(const ClippingArgs& args, Dir currentDown) const;
	
	PickWallResult PickWall(const eg::Ray& ray) const;
	
private:
	void BuildVoxelMesh(std::vector<WallVertex>& verticesOut, std::vector<uint16_t>& indicesOut) const;
	
	/**
	 * Each voxel is represented by one 64-bit integer where the low 8 * 6 bits store which texture is used
	 * for each side (with 8 bits per side) and the 48th bit is set if the voxel is air.
	 */
	std::unique_ptr<uint64_t[]> m_voxelBuffer;
	glm::ivec3 m_voxelBufferMin;
	glm::ivec3 m_voxelBufferMax;
	
	bool m_voxelBufferOutOfDate = true;
	
	eg::Buffer m_voxelVertexBuffer;
	eg::Buffer m_voxelIndexBuffer;
	size_t m_voxelVertexBufferCapacity = 0;
	size_t m_voxelIndexBufferCapacity = 0;
	uint32_t m_numVoxelIndices = 0;
	
	std::vector<GravityCorner> m_gravityCorners;
};
