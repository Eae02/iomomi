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
	
	const eg::ColorLin& EditorColor() const
	{
		return m_editorColor;
	}
	
	void SetEditorColor(const eg::ColorLin& editorColor)
	{
		m_editorColor = editorColor;
	}
	
	bool ShouldGenerateMesh() const
	{
		return m_shouldGenerateMesh;
	}
	
	void SetShouldGenerateMesh(bool shouldGenerateMesh)
	{
		m_shouldGenerateMesh = shouldGenerateMesh;
	}
	
	bool OutOfDate() const
	{
		return m_outOfDate;
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
	
private:
	void GenerateMesh();
	
	std::vector<glm::ivec3> m_underwater;
	
	eg::ColorLin m_editorColor;
	
	bool m_outOfDate = true;
	bool m_shouldGenerateMesh = false;
	size_t m_verticesCapacity = 0;
	size_t m_indicesCapacity = 0;
	uint32_t m_numIndices = 0;
	eg::Buffer m_vertexBuffer;
	eg::Buffer m_indexBuffer;
};
