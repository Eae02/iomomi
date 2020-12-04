#include "PhysicsDebugRenderer.hpp"
#include "../World/PhysicsEngine.hpp"

PhysicsDebugRenderer::PhysicsDebugRenderer()
{
	m_vertexBuffer.flags = eg::BufferFlags::VertexBuffer | eg::BufferFlags::CopyDst;
	m_indexBuffer.flags = eg::BufferFlags::IndexBuffer | eg::BufferFlags::CopyDst;
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Primitive.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Primitive.fs.glsl").DefaultVariant();
	pipelineCI.vertexBindings[0] = { sizeof(PhysicsDebugVertex), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, (uint32_t)offsetof(PhysicsDebugVertex, x) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::UInt8Norm, 4, (uint32_t)offsetof(PhysicsDebugVertex, color) };
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
	pipelineCI.lineWidth = 2;
	pipelineCI.blendStates[0] = eg::AlphaBlend;
	
	pipelineCI.topology = eg::Topology::TriangleList;
	pipelineCI.wireframe = true;
	m_trianglePipeline = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.topology = eg::Topology::LineList;
	pipelineCI.wireframe = false;
	m_linesPipeline = eg::Pipeline::Create(pipelineCI);
}

void PhysicsDebugRenderer::Render(const class PhysicsEngine& physicsEngine, const glm::mat4& viewProjTransform,
	eg::TextureRef colorFBTex, eg::TextureRef depthFBTex)
{
	m_renderData.vertices.clear();
	m_renderData.triangleIndices.clear();
	m_renderData.lineIndices.clear();
	physicsEngine.GetDebugRenderData(m_renderData);
	Render(m_renderData, viewProjTransform, colorFBTex, depthFBTex);
}

void PhysicsDebugRenderer::Render(const PhysicsDebugRenderData& renderData, const glm::mat4& viewProjTransform,
	eg::TextureRef colorFBTex, eg::TextureRef depthFBTex)
{
	if ((renderData.lineIndices.empty() && renderData.triangleIndices.empty()) || renderData.vertices.empty())
		return;
	
	const uint32_t numLineIndices = renderData.lineIndices.size();
	const uint32_t numTriangleIndices = renderData.triangleIndices.size();
	
	m_vertexBuffer.EnsureSize(renderData.vertices.size(), sizeof(PhysicsDebugVertex));
	m_indexBuffer.EnsureSize(numLineIndices + numTriangleIndices, sizeof(uint32_t));
	
	//Gets a staging buffer
	uint64_t verticesBytes = renderData.vertices.size() * sizeof(PhysicsDebugVertex);
	uint64_t lineIndicesBytes = numLineIndices * sizeof(uint32_t);
	uint64_t triangleIndicesBytes = numTriangleIndices * sizeof(uint32_t);
	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(verticesBytes + lineIndicesBytes + triangleIndicesBytes);
	
	//Copies data to the staging buffer
	char* uploadMem = (char*)uploadBuffer.Map();
	std::memcpy(uploadMem, renderData.vertices.data(), verticesBytes);
	std::memcpy(uploadMem + verticesBytes, renderData.lineIndices.data(), lineIndicesBytes);
	std::memcpy(uploadMem + verticesBytes + lineIndicesBytes, renderData.triangleIndices.data(), triangleIndicesBytes);
	uploadBuffer.Flush();
	
	//Uploads data to the device buffers
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_vertexBuffer.buffer, uploadBuffer.offset, 0, verticesBytes);
	eg::DC.CopyBuffer(uploadBuffer.buffer, m_indexBuffer.buffer, uploadBuffer.offset + verticesBytes,
		0, lineIndicesBytes + triangleIndicesBytes);
	
	m_vertexBuffer.buffer.UsageHint(eg::BufferUsage::VertexBuffer);
	m_indexBuffer.buffer.UsageHint(eg::BufferUsage::IndexBuffer);
	
	//Recreates the framebuffer if the framebuffer textures don't match
	if (colorFBTex.handle != m_prevColorFBTex.handle || depthFBTex.handle != m_prevDepthFBTex.handle)
	{
		eg::FramebufferAttachment colorAttachment;
		colorAttachment.texture = colorFBTex.handle;
		m_framebuffer = eg::Framebuffer({ &colorAttachment, 1 }, depthFBTex.handle);
	}
	
	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.framebuffer = m_framebuffer.handle;
	rpBeginInfo.colorAttachments[0].loadOp = eg::AttachmentLoadOp::Load;
	rpBeginInfo.depthLoadOp = eg::AttachmentLoadOp::Load;
	eg::DC.BeginRenderPass(rpBeginInfo);
	
	glm::mat4 updatedViewProj = glm::translate(glm::mat4(1), glm::vec3(0, 0, -0.0001f)) * viewProjTransform;
	
	if (numLineIndices != 0)
	{
		eg::DC.BindPipeline(m_linesPipeline);
		eg::DC.PushConstants(0, updatedViewProj);
		eg::DC.BindVertexBuffer(0, m_vertexBuffer.buffer, 0);
		eg::DC.BindIndexBuffer(eg::IndexType::UInt32, m_indexBuffer.buffer, 0);
		eg::DC.DrawIndexed(0, numLineIndices, 0, 0, 1);
	}
	
	if (numTriangleIndices != 0)
	{
		eg::DC.BindPipeline(m_trianglePipeline);
		eg::DC.PushConstants(0, updatedViewProj);
		eg::DC.BindVertexBuffer(0, m_vertexBuffer.buffer, 0);
		eg::DC.BindIndexBuffer(eg::IndexType::UInt32, m_indexBuffer.buffer, 0);
		eg::DC.DrawIndexed(numLineIndices, numTriangleIndices, 0, 0, 1);
	}
	
	eg::DC.EndRenderPass();
}
