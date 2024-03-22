#include "PhysicsDebugRenderer.hpp"

#include "../World/PhysicsEngine.hpp"
#include "RenderTargets.hpp"

PhysicsDebugRenderer::PhysicsDebugRenderer()
{
	m_vertexBuffer.flags = eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst;
	m_indexBuffer.flags = eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst;

	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Primitive.vs.glsl").ToStageInfo();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Primitive.fs.glsl").ToStageInfo();
	pipelineCI.vertexBindings[0] = { sizeof(PhysicsDebugVertex), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = eg::VertexAttribute(0, eg::DataType::Float32, 3, offsetof(PhysicsDebugVertex, x));
	pipelineCI.vertexAttributes[1] =
		eg::VertexAttribute(0, eg::DataType::UInt8Norm, 4, offsetof(PhysicsDebugVertex, color));
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.lineWidth = 2;
	pipelineCI.blendStates[0] = eg::AlphaBlend;
	pipelineCI.numColorAttachments = 1;
	pipelineCI.colorAttachmentFormats[0] = lightColorAttachmentFormat;
	pipelineCI.depthAttachmentFormat = GB_DEPTH_FORMAT;
	pipelineCI.depthStencilUsage = eg::TextureUsage::DepthStencilReadOnly;
	pipelineCI.label = "PhysicsDebugRenderer";

	pipelineCI.topology = eg::Topology::TriangleList;
	pipelineCI.enableWireframeRasterization = true;
	m_trianglePipeline = eg::Pipeline::Create(pipelineCI);

	pipelineCI.topology = eg::Topology::LineList;
	pipelineCI.enableWireframeRasterization = false;
	m_linesPipeline = eg::Pipeline::Create(pipelineCI);

	m_viewProjBuffer =
		eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::UniformBuffer, sizeof(glm::mat4), nullptr);
	m_descriptorSet = eg::DescriptorSet(m_trianglePipeline, 0);
	m_descriptorSet.BindUniformBuffer(m_viewProjBuffer, 0);
}

void PhysicsDebugRenderer::Prepare(const class PhysicsEngine& physicsEngine, const glm::mat4& viewProjTransform)
{
	m_renderData.vertices.clear();
	m_renderData.triangleIndices.clear();
	m_renderData.lineIndices.clear();
	physicsEngine.GetDebugRenderData(m_renderData);
	Prepare(m_renderData, viewProjTransform);
}

void PhysicsDebugRenderer::Prepare(const PhysicsDebugRenderData& renderData, const glm::mat4& viewProjTransform)
{
	if ((renderData.lineIndices.empty() && renderData.triangleIndices.empty()) || renderData.vertices.empty())
		return;

	m_numLineIndices = renderData.lineIndices.size();
	m_numTriangleIndices = renderData.triangleIndices.size();

	m_vertexBuffer.EnsureSize(renderData.vertices.size(), sizeof(PhysicsDebugVertex));
	m_indexBuffer.EnsureSize(m_numLineIndices + m_numTriangleIndices, sizeof(uint32_t));

	// Gets a staging buffer
	uint64_t verticesBytes = renderData.vertices.size() * sizeof(PhysicsDebugVertex);
	uint64_t lineIndicesBytes = m_numLineIndices * sizeof(uint32_t);
	uint64_t triangleIndicesBytes = m_numTriangleIndices * sizeof(uint32_t);
	eg::UploadBuffer uploadBuffer =
		eg::GetTemporaryUploadBuffer(verticesBytes + lineIndicesBytes + triangleIndicesBytes);

	// Copies data to the staging buffer
	char* uploadMem = (char*)uploadBuffer.Map();
	std::memcpy(uploadMem, renderData.vertices.data(), verticesBytes);
	std::memcpy(uploadMem + verticesBytes, renderData.lineIndices.data(), lineIndicesBytes);
	std::memcpy(uploadMem + verticesBytes + lineIndicesBytes, renderData.triangleIndices.data(), triangleIndicesBytes);
	uploadBuffer.Flush();

	// Uploads data to the device buffers
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_vertexBuffer.buffer, uploadBuffer.offset, 0, verticesBytes);
	eg::DC.CopyBuffer(
		uploadBuffer.buffer, m_indexBuffer.buffer, uploadBuffer.offset + verticesBytes, 0,
		lineIndicesBytes + triangleIndicesBytes
	);

	m_vertexBuffer.buffer.UsageHint(eg::BufferUsage::VertexBuffer);
	m_indexBuffer.buffer.UsageHint(eg::BufferUsage::IndexBuffer);

	glm::mat4 updatedViewProj = glm::translate(glm::mat4(1), glm::vec3(0, 0, -0.0001f)) * viewProjTransform;
	m_viewProjBuffer.DCUpdateData(0, sizeof(glm::mat4), &updatedViewProj);
	m_viewProjBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Vertex);
}

void PhysicsDebugRenderer::Render()
{
	if (m_numLineIndices != 0)
	{
		eg::DC.BindPipeline(m_linesPipeline);
		eg::DC.BindDescriptorSet(m_descriptorSet, 0);
		eg::DC.BindVertexBuffer(0, m_vertexBuffer.buffer, 0);
		eg::DC.BindIndexBuffer(eg::IndexType::UInt32, m_indexBuffer.buffer, 0);
		eg::DC.DrawIndexed(0, m_numLineIndices, 0, 0, 1);
	}

	if (m_numTriangleIndices != 0)
	{
		eg::DC.BindPipeline(m_trianglePipeline);
		eg::DC.SetWireframe(true);
		eg::DC.BindDescriptorSet(m_descriptorSet, 0);
		eg::DC.BindVertexBuffer(0, m_vertexBuffer.buffer, 0);
		eg::DC.BindIndexBuffer(eg::IndexType::UInt32, m_indexBuffer.buffer, 0);
		eg::DC.DrawIndexed(m_numLineIndices, m_numTriangleIndices, 0, 0, 1);
	}
}
