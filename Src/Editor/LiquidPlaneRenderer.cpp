#include "LiquidPlaneRenderer.hpp"

#include "../Graphics/RenderSettings.hpp"
#include "../World/Entities/Components/LiquidPlaneComp.hpp"
#include "../World/World.hpp"
#include "EditorGraphics.hpp"

LiquidPlaneRenderer::LiquidPlaneRenderer()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LiquidPlaneEditor.vs.glsl").ToStageInfo();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LiquidPlaneEditor.fs.glsl").ToStageInfo();
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.vertexBindings[0] = { sizeof(LiquidPlaneComp::Vertex), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(LiquidPlaneComp::Vertex, pos) };
	pipelineCI.blendStates[0] = eg::AlphaBlend;
	pipelineCI.colorAttachmentFormats[0] = EDITOR_COLOR_FORMAT;
	pipelineCI.depthAttachmentFormat = EDITOR_DEPTH_FORMAT;
	pipelineCI.label = "EdLiquidPlane";

	m_pipeline = eg::Pipeline::Create(pipelineCI);

	m_descriptorSet = eg::DescriptorSet(m_pipeline, 0);
	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0);
	m_descriptorSet.BindUniformBuffer(frameDataUniformBuffer, 1, eg::BIND_BUFFER_OFFSET_DYNAMIC, sizeof(float) * 4);
}

void LiquidPlaneRenderer::Prepare(World& world, eg::MeshBatchOrdered& meshBatch, const glm::vec3& cameraPos)
{
	m_planes.clear();

	world.entManager.ForEach(
		[&](Ent& entity)
		{
			LiquidPlaneComp* plane = entity.GetComponentMut<LiquidPlaneComp>();
			if (plane == nullptr)
				return;

			plane->shouldGenerateMesh = true;
			plane->MaybeUpdate(world);

			if (plane->NumIndices() != 0)
			{
				float distToCamera = std::abs(plane->position.y - cameraPos.y);
				m_planes.emplace_back(distToCamera, plane);
			}
		}
	);
}

void LiquidPlaneRenderer::Render() const
{
	if (m_planes.empty())
		return;

	eg::DC.BindPipeline(m_pipeline);

	for (int64_t i = eg::ToInt64(m_planes.size()) - 1; i >= 0; i--)
	{
		const LiquidPlaneComp& plane = *m_planes[i].second;

		eg::DC.BindVertexBuffer(0, plane.VertexBuffer(), 0);
		eg::DC.BindIndexBuffer(eg::IndexType::UInt16, plane.IndexBuffer(), 0);

		uint32_t dataOffset = PushFrameUniformData(RefToCharSpan(plane.editorColor));
		eg::DC.BindDescriptorSet(m_descriptorSet, 0, { &dataOffset, 1 });

		eg::DC.DrawIndexed(0, plane.NumIndices(), 0, 0, 1);
	}
}
