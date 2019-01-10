#include "World.hpp"
#include "Voxel.hpp"
#include "../Graphics/Mesh.hpp"
#include "../Graphics/Graphics.hpp"

World::World()
{
	
}

inline int VoxelBufferIdx(const glm::ivec3& pos, const glm::ivec3& min, const glm::ivec3& max)
{
	glm::ivec3 rel = pos - min;
	glm::ivec3 size = (max - min) + glm::ivec3(1);
	if (rel.x < 0 || rel.y < 0 || rel.z < 0 || rel.x >= size.x || rel.y >= size.y || rel.z >= size.z)
		return -1;
	return rel.x + rel.y * size.x + rel.z * size.x * size.y;
}

void World::SetIsAir(const glm::ivec3& pos, bool isAir)
{
	if (IsAir(pos) == isAir)
		return;
	
	int idx = VoxelBufferIdx(pos, m_voxelBufferMin, m_voxelBufferMax);
	if (idx != -1)
	{
		m_voxelBuffer.get()[idx].isAir = isAir;
		m_voxelBufferOutOfDate = true;
		return;
	}
	
	// ** Expands the voxel buffer **
	
	constexpr float EXPAND_STEP = 32;
	
	glm::ivec3 oldMin = m_voxelBufferMin;
	glm::ivec3 oldMax = m_voxelBufferMax;
	m_voxelBufferMin = m_voxelBuffer ? glm::min(m_voxelBufferMin, pos) : pos;
	m_voxelBufferMax = m_voxelBuffer ? glm::max(m_voxelBufferMax, pos) : pos;
	m_voxelBufferMin = glm::floor(glm::vec3(m_voxelBufferMin) / EXPAND_STEP) * EXPAND_STEP;
	m_voxelBufferMax = glm::ceil(glm::vec3(m_voxelBufferMax) / EXPAND_STEP) * EXPAND_STEP;
	
	//Allocates a new voxel buffer
	size_t bufferSize = (size_t)(m_voxelBufferMax.x - m_voxelBufferMin.x + 1) *
		(size_t)(m_voxelBufferMax.y - m_voxelBufferMin.y + 1) *
		(size_t)(m_voxelBufferMax.z - m_voxelBufferMin.z + 1);
	std::unique_ptr<Voxel[]> newBuffer = std::make_unique<Voxel[]>(bufferSize);
	
	//Copies voxel buffer data to the new buffer
	if (m_voxelBuffer != nullptr)
	{
		for (int z = m_voxelBufferMin.z; z <= m_voxelBufferMax.z; z++)
		{
			for (int y = m_voxelBufferMin.y; y <= m_voxelBufferMax.y; y++)
			{
				for (int x = m_voxelBufferMin.x; x <= m_voxelBufferMax.x; x++)
				{
					const int srcIdx = VoxelBufferIdx({x, y, z}, oldMin, oldMax);
					if (srcIdx != -1)
					{
						const int dstIdx = VoxelBufferIdx({x, y, z}, m_voxelBufferMin, m_voxelBufferMax);
						newBuffer[dstIdx] = m_voxelBuffer[srcIdx];
					}
				}
			}
		}
	}
	
	m_voxelBuffer = std::move(newBuffer);
	SetIsAir(pos, isAir);
}

bool World::IsAir(const glm::ivec3& pos) const
{
	if (m_voxelBuffer == nullptr)
		return false;
	int idx = VoxelBufferIdx(pos, m_voxelBufferMin, m_voxelBufferMax);
	return idx == -1 ? false : m_voxelBuffer[idx].isAir;
}

void World::Draw(uint32_t vFrameIndex)
{
	if (m_voxelBufferOutOfDate)
	{
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices;
		BuildVoxelMesh(vertices, indices);
		
		if (m_voxelVertexBufferCapacity < vertices.size())
		{
			m_voxelVertexBufferCapacity = eg::RoundToNextMultiple(vertices.size(), 1024);
			//m_voxelVertexBuffer.Realloc(m_voxelVertexBufferCapacity * NUM_VIRTUAL_FRAMES * sizeof(Vertex), nullptr);
		}
		
		if (m_voxelIndexBufferCapacity < indices.size())
		{
			m_voxelIndexBufferCapacity = eg::RoundToNextMultiple(indices.size(), 1024);
			//m_voxelIndexBuffer.Realloc(m_voxelIndexBufferCapacity * NUM_VIRTUAL_FRAMES * sizeof(uint16_t), nullptr);
		}
		
		m_voxelIndexBufferOffset = m_voxelIndexBufferCapacity * sizeof(uint16_t) * vFrameIndex;
		//size_t vertexBufferOffset = m_voxelVertexBufferCapacity * sizeof(Vertex) * vFrameIndex;
		
		//m_voxelVertexBuffer.Update(vertexBufferOffset, vertices.size() * sizeof(Vertex), vertices.data());
		//m_voxelIndexBuffer.Update(m_voxelIndexBufferOffset, indices.size() * sizeof(uint16_t), indices.data());
		
		m_numVoxelIndices = (int)indices.size();
		m_voxelBufferOutOfDate = false;
	}
	
	//glDrawElements(GL_TRIANGLES, m_numVoxelIndices, GL_UNSIGNED_SHORT, (void*)m_voxelIndexBufferOffset);
}

void World::BuildVoxelMesh(std::vector<Vertex>& verticesOut, std::vector<uint16_t>& indicesOut) const
{
	for (int z = m_voxelBufferMin.z; z <= m_voxelBufferMax.z; z++)
	{
		for (int y = m_voxelBufferMin.y; y <= m_voxelBufferMax.y; y++)
		{
			for (int x = m_voxelBufferMin.x; x <= m_voxelBufferMax.x; x++)
			{
				//Only emit triangles for solid voxels
				int idx = VoxelBufferIdx({ x, y, z }, m_voxelBufferMin, m_voxelBufferMax);
				if (m_voxelBuffer[idx].isAir)
					continue;
				
				for (int s = 0; s < 6; s++)
				{
					const glm::ivec3 nPos = glm::ivec3(x, y, z) + voxel::normals[s];
					const int nIdx = VoxelBufferIdx(nPos, m_voxelBufferMin, m_voxelBufferMax);
					
					//Only emit triangles for voxels that face into an air voxel
					if (nIdx == -1 || !m_voxelBuffer[nIdx].isAir)
						continue;
					
					//The center of the face to be emitted
					const glm::vec3 faceCenter = glm::vec3(x, y, z) + glm::vec3(voxel::normals[s]) * 0.5f;
					
					//Emits indices
					const uint16_t baseIndex = (uint16_t)verticesOut.size();
					static const uint16_t indicesRelative[] = { 0, 1, 2, 2, 1, 3 };
					for (uint16_t i : indicesRelative)
						indicesOut.push_back(baseIndex + i);
					
					//Emits vertices
					glm::vec3 tangent = voxel::tangents[s];
					glm::vec3 biTangent = voxel::biTangents[s];
					for (int fx = 0; fx < 2; fx++)
					{
						for (int fy = 0; fy < 2; fy++)
						{
							Vertex& vertex = verticesOut.emplace_back();
							vertex.position = faceCenter + tangent * (fy - 0.5f) + biTangent * (fx - 0.5f);
							vertex.texCoord = glm::vec2(fx, 1 - fy);
							vertex.SetNormal(voxel::normals[s]);
							vertex.SetTangent(tangent);
						}
					}
				}
			}
		}
	}
}
