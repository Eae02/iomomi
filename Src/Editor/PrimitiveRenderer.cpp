#include "PrimitiveRenderer.hpp"

#include "../Graphics/DeferredRenderer.hpp"

eg::Pipeline primPipeline;

void PrimitiveRenderer::OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Primitive.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Primitive.fs.glsl").DefaultVariant();
	pipelineCI.vertexBindings[0] = { sizeof(Vertex), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = eg::VertexAttribute(0, eg::DataType::Float32, 3, offsetof(Vertex, position));
	pipelineCI.vertexAttributes[1] = eg::VertexAttribute(0, eg::DataType::UInt8Norm, 4, offsetof(Vertex, color));
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.blendStates[0] = eg::AlphaBlend;
	pipelineCI.label = "EdPrimitive";

	primPipeline = eg::Pipeline::Create(pipelineCI);
	primPipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
}

void PrimitiveRenderer::OnShutdown()
{
	primPipeline.Destroy();
}

EG_ON_INIT(PrimitiveRenderer::OnInit)
EG_ON_SHUTDOWN(PrimitiveRenderer::OnShutdown)

void PrimitiveRenderer::Begin(const glm::mat4& viewProjection, const glm::vec3& cameraPos)
{
	m_viewProjection = viewProjection;
	m_cameraPos = cameraPos;
	m_triangles.clear();
	m_vertices.clear();
}

void PrimitiveRenderer::AddVertex(const glm::vec3& position, const eg::ColorSRGB& color)
{
	Vertex& vertex = m_vertices.emplace_back();
	vertex.position = position;
	vertex.color[0] = static_cast<uint8_t>(color.r * 255.0f);
	vertex.color[1] = static_cast<uint8_t>(color.g * 255.0f);
	vertex.color[2] = static_cast<uint8_t>(color.b * 255.0f);
	vertex.color[3] = static_cast<uint8_t>(color.a * 255.0f);
}

void PrimitiveRenderer::AddTriangle(uint32_t v0, uint32_t v1, uint32_t v2)
{
	glm::vec3 midPos = m_vertices[v0].position + m_vertices[v1].position + m_vertices[v2].position;

	Triangle& triangle = m_triangles.emplace_back();
	triangle.indices[0] = v0;
	triangle.indices[1] = v1;
	triangle.indices[2] = v2;
	triangle.depth = (m_viewProjection * glm::vec4(midPos, 1.0f)).z;
}

void PrimitiveRenderer::AddTriangle(const glm::vec3* positions, const eg::ColorSRGB& color)
{
	uint32_t baseIdx = NextIndex();
	for (int i = 0; i < 3; i++)
		AddVertex(positions[i], color);
	AddTriangle(baseIdx + 0, baseIdx + 1, baseIdx + 2);
}

void PrimitiveRenderer::AddQuad(const glm::vec3* positions, const eg::ColorSRGB& color)
{
	uint32_t baseIdx = NextIndex();
	for (int i = 0; i < 4; i++)
		AddVertex(positions[i], color);
	AddTriangle(baseIdx + 0, baseIdx + 1, baseIdx + 2);
	AddTriangle(baseIdx + 1, baseIdx + 2, baseIdx + 3);
}

void PrimitiveRenderer::AddLine(const glm::vec3& a, const glm::vec3& b, const eg::ColorSRGB& color, float width)
{
	const glm::vec3 side = glm::normalize(glm::cross(a - m_cameraPos, b - m_cameraPos)) * (width * 0.5f);
	const glm::vec3 quadPositions[] = { a - side, b - side, a + side, b + side };
	AddQuad(quadPositions, color);
}

void PrimitiveRenderer::AddCollisionMesh(const eg::CollisionMesh& mesh, const eg::ColorSRGB& color)
{
	uint32_t baseIndex = eg::UnsignedNarrow<uint32_t>(m_vertices.size());
	for (size_t v = 0; v < mesh.NumVertices(); v++)
	{
		AddVertex(mesh.Vertex(v), color);
	}

	for (size_t i = 0; i < mesh.NumIndices(); i += 3)
	{
		AddTriangle(
			baseIndex + mesh.Indices()[i], baseIndex + mesh.Indices()[i + 1], baseIndex + mesh.Indices()[i + 2]);
	}
}

void PrimitiveRenderer::End()
{
	if (m_triangles.empty())
		return;

	// Sorts triangles by depth
	std::sort(
		m_triangles.begin(), m_triangles.end(), [](const Triangle& a, const Triangle& b) { return a.depth < b.depth; });

	const uint64_t verticesBytes = m_vertices.size() * sizeof(Vertex);
	const uint64_t indicesBytes = m_triangles.size() * 3 * sizeof(uint32_t);

	// Reallocates the vertex buffer if it's currently too small
	if (m_vertexBufferCapacity < verticesBytes)
	{
		m_vertexBufferCapacity = eg::RoundToNextMultiple(verticesBytes, 1024 * sizeof(Vertex));
		m_vertexBuffer =
			eg::Buffer(eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst, m_vertexBufferCapacity, nullptr);
	}

	// Reallocates the index buffer if it's currently too small
	if (m_indexBufferCapacity < indicesBytes)
	{
		m_indexBufferCapacity = eg::RoundToNextMultiple(indicesBytes, 1024 * 3 * sizeof(uint32_t));
		m_indexBuffer =
			eg::Buffer(eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst, m_indexBufferCapacity, nullptr);
	}

	// Allocates an upload buffer and copies vertices and indices to it
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(verticesBytes + indicesBytes);
	char* uploadMem = reinterpret_cast<char*>(uploadBuffer.Map());
	std::memcpy(uploadMem, m_vertices.data(), verticesBytes);

	uint32_t* indicesOut = reinterpret_cast<uint32_t*>(uploadMem + verticesBytes);
	for (size_t i = 0; i < m_triangles.size(); i++)
	{
		for (size_t j = 0; j < 3; j++)
			indicesOut[3 * i + j] = m_triangles[i].indices[j];
	}

	uploadBuffer.Flush();

	// Uploads vertices and indices to the GPU
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_vertexBuffer, uploadBuffer.offset, 0, verticesBytes);
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_indexBuffer, uploadBuffer.offset + verticesBytes, 0, indicesBytes);

	m_vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	m_indexBuffer.UsageHint(eg::BufferUsage::IndexBuffer);
}

void PrimitiveRenderer::Draw() const
{
	if (m_triangles.empty())
		return;

	eg::DC.BindPipeline(primPipeline);

	eg::DC.PushConstants(0, m_viewProjection);

	eg::DC.BindIndexBuffer(eg::IndexType::UInt32, m_indexBuffer, 0);
	eg::DC.BindVertexBuffer(0, m_vertexBuffer, 0);

	eg::DC.DrawIndexed(0, eg::UnsignedNarrow<uint32_t>(m_triangles.size()) * 3, 0, 0, 1);
}
