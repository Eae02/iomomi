#pragma once

#include <cstddef>

#include <EGame/EG.hpp>

struct Vertex;

class World
{
public:
	World();
	
	void SetIsAir(const glm::ivec3& pos, bool isAir);
	
	bool IsAir(const glm::ivec3& pos) const;
	
	void Draw(uint32_t vFrameIndex);
	
private:
	struct Voxel
	{
		bool isAir;
	};
	
	void BuildVoxelMesh(std::vector<Vertex>& verticesOut, std::vector<uint16_t>& indicesOut) const;
	
	std::unique_ptr<Voxel[]> m_voxelBuffer;
	glm::ivec3 m_voxelBufferMin;
	glm::ivec3 m_voxelBufferMax;
	
	bool m_voxelBufferOutOfDate = true;
	
	eg::Buffer m_voxelVertexBuffer;
	eg::Buffer m_voxelIndexBuffer;
	size_t m_voxelVertexBufferCapacity = 0;
	size_t m_voxelIndexBufferCapacity = 0;
	size_t m_voxelIndexBufferOffset = 0;
	int m_numVoxelIndices = 0;
};
