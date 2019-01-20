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
	
	void AddGravityCorner(const GravityCorner& corner)
	{
		m_gravityCorners.push_back(corner);
	}
	
	void SetIsAir(const glm::ivec3& pos, bool isAir);
	
	bool IsAir(const glm::ivec3& pos) const;
	
	void PrepareForDraw(class ObjectRenderer& objectRenderer);
	
	void Draw(const class RenderSettings& renderSettings, const class WallShader& shader);
	
	void CalcClipping(ClippingArgs& args) const;
	
	const GravityCorner* FindGravityCorner(const ClippingArgs& args, Dir currentDown) const;
	
private:
	struct Voxel
	{
		bool isAir;
	};
	
	void BuildVoxelMesh(std::vector<WallVertex>& verticesOut, std::vector<uint16_t>& indicesOut) const;
	
	std::unique_ptr<Voxel[]> m_voxelBuffer;
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
