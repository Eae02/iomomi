#pragma once

#include "Vertex.hpp"

class Mesh
{
public:
	Mesh(eg::Span<const Vertex> vertices, eg::Span<const uint32_t> indices);
	
	void Draw(eg::CommandContext& ctx) const;
	
private:
	eg::Buffer m_vertexBuffer;
	eg::Buffer m_indexBuffer;
	//eg::VertexArray m_vertexArray;
	
	uint32_t m_numIndices;
};

class NamedMesh : public Mesh
{
public:
	NamedMesh(std::string name, eg::Span<const Vertex> vertices, eg::Span<const uint32_t> indices)
		: Mesh(vertices, indices), m_name(std::move(name)) { }
	
	const std::string& Name() const
	{
		return m_name;
	}
	
private:
	std::string m_name;
};
