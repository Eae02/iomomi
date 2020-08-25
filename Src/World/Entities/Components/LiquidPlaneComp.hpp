#pragma once

#include "../Entity.hpp"

class LiquidPlaneComp
{
public:
	LiquidPlaneComp() = default;
	
	void MaybeUpdate(const class World& world);
	
	bool IsUnderwater(const glm::ivec3& pos) const;
	bool IsUnderwater(const glm::vec3& pos) const;
	bool IsUnderwater(const eg::Sphere& sphere) const;
	
	const std::vector<glm::ivec3> UnderwaterCells() const
	{
		return m_underwater;
	}
	
	bool OutOfDate() const
	{
		return m_outOfDate;
	}
	
	void MarkOutOfDate()
	{
		m_outOfDate = true;
	}
	
	uint32_t NumIndices() const
	{
		return m_numIndices;
	}
	
	const eg::Buffer& VertexBuffer() const
	{
		return m_vertexBuffer;
	}
	
	const eg::Buffer& IndexBuffer() const
	{
		return m_indexBuffer;
	}
	
	eg::MeshBatch::Mesh GetMesh() const;
	
	glm::vec3 position;
	Dir wallForward;
	
	bool shouldGenerateMesh;
	eg::ColorLin editorColor;
	
private:
	void GenerateMesh();
	
	std::vector<glm::ivec3> m_underwater;
	
	bool m_outOfDate = true;
	size_t m_verticesCapacity = 0;
	size_t m_indicesCapacity = 0;
	uint32_t m_numIndices = 0;
	eg::Buffer m_vertexBuffer;
	eg::Buffer m_indexBuffer;
};
