#include "Mesh.hpp"
#include "GL/GL.hpp"

Mesh::Mesh(Span<const Vertex> vertices, Span<const uint32_t> indices)
	: m_numIndices((int)indices.size())
{
	glGenBuffers(1, m_vertexBuffer.GenPtr());
	glGenBuffers(1, m_indexBuffer.GenPtr());
	glGenVertexArrays(1, m_vertexArray.GenPtr());
	
	glBindVertexArray(*m_vertexArray);
	
	glBindBuffer(GL_ARRAY_BUFFER, *m_vertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *m_indexBuffer);
	
	if (glBufferStorage)
	{
		glBufferStorage(GL_ARRAY_BUFFER, vertices.SizeBytes(), vertices.data(), 0);
		glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, indices.SizeBytes(), indices.data(), 0);
	}
	else
	{
		glBufferData(GL_ARRAY_BUFFER, vertices.SizeBytes(), vertices.data(), GL_STATIC_DRAW);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.SizeBytes(), indices.data(), GL_STATIC_DRAW);
	}
	
	for (GLuint i = 0; i < 4; i++)
		glEnableVertexAttribArray(i);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
	glVertexAttribPointer(2, 3, GL_BYTE,  GL_TRUE,  sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glVertexAttribPointer(3, 3, GL_BYTE,  GL_TRUE,  sizeof(Vertex), (void*)offsetof(Vertex, tangent));
}

void Mesh::Draw() const
{
	glBindVertexArray(*m_vertexArray);
	glDrawElements(GL_TRIANGLES, m_numIndices, GL_UNSIGNED_INT, nullptr);
}
