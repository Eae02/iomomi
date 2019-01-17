#include "Mesh.hpp"

Mesh::Mesh(eg::Span<const Vertex> vertices, eg::Span<const uint32_t> indices)
	: m_vertexBuffer(eg::BufferUsage::VertexBuffer, eg::MemoryType::DeviceLocal, vertices.SizeBytes(), vertices.data()),
	  m_indexBuffer(eg::BufferUsage::IndexBuffer, eg::MemoryType::DeviceLocal, indices.SizeBytes(), indices.data()),
	  m_numIndices(indices.size())
{
	
}

void Mesh::Draw(eg::CommandContext& ctx) const
{
	ctx.BindVertexBuffer(0, m_vertexBuffer, 0);
	ctx.BindIndexBuffer(eg::IndexType::UInt32, m_indexBuffer, 0);
	ctx.DrawIndexed(0, m_numIndices, 0, 0);
}
