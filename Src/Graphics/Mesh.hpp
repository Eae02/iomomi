#pragma once

#include "GL/Handle.hpp"
#include "../Span.hpp"

struct Vertex
{
	glm::vec3 position;
	glm::vec2 texCoord;
	int8_t normal[4];
	int8_t tangent[4];
};

class Mesh
{
public:
	Mesh(Span<const Vertex> vertices, Span<const uint32_t> indices);
	
	void Draw() const;
	
private:
	gl::Handle<gl::ResType::Buffer> m_vertexBuffer;
	gl::Handle<gl::ResType::Buffer> m_indexBuffer;
	gl::Handle<gl::ResType::VertexArray> m_vertexArray;
	
	int m_numIndices;
};

class NamedMesh : public Mesh
{
public:
	NamedMesh(std::string name, Span<const Vertex> vertices, Span<const uint32_t> indices)
		: Mesh(vertices, indices), m_name(std::move(name)) { }
	
	const std::string& Name() const
	{
		return m_name;
	}
	
private:
	std::string m_name;
};
