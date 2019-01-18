#pragma once

#include <cstddef>

struct WallVertex;

struct ClippingArgs
{
	glm::vec3 aabbMin;
	glm::vec3 aabbMax;
	glm::vec3 move;
	glm::vec3 colPlaneNormal;
	float clipDist;
};

class World
{
public:
	World();
	
	void SetIsAir(const glm::ivec3& pos, bool isAir);
	
	bool IsAir(const glm::ivec3& pos) const;
	
	void PrepareForDraw();
	
	void Draw(const class RenderSettings& renderSettings, const class WallShader& shader);
	
	void CalcClipping(ClippingArgs& args) const;
	
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
};
