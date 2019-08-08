#pragma once

#include "ECEditorVisible.hpp"

class ECLiquidPlane
{
public:
	ECLiquidPlane() = default;
	
	void HandleMessage(eg::Entity& entity, const EditorSpawnedMessage& message);
	void HandleMessage(eg::Entity& entity, const EditorWallsChangedMessage& message);
	void HandleMessage(eg::Entity& entity, const EditorMovedMessage& message);
	
	static eg::MessageReceiver MessageReceiver;
	
	void MaybeUpdate(const eg::Entity& entity, const class World& world);
	
	bool IsUnderwater(const glm::ivec3& pos);
	
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
	
private:
	void GenerateMesh(const eg::Entity& entity);
	
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
