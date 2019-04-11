#include "LiquidPlaneRenderer.hpp"
#include "../World/World.hpp"
#include "../World/Entities/ECLiquidPlane.hpp"
#include "../Graphics/RenderSettings.hpp"

LiquidPlaneRenderer::LiquidPlaneRenderer()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LiquidPlaneEditor.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/LiquidPlaneEditor.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.vertexBindings[0] = { sizeof(glm::vec3), eg::InputRate::Vertex };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
	pipelineCI.blendStates[0] = eg::AlphaBlend;
	
	m_pipeline = eg::Pipeline::Create(pipelineCI);
	m_pipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
}

static const eg::EntitySignature liquidPlaneSig = eg::EntitySignature::Create<ECLiquidPlane>();

void LiquidPlaneRenderer::Prepare(const World& world, const glm::vec3& cameraPos)
{
	m_planes.clear();
	
	for (eg::Entity& entity : world.EntityManager().GetEntitySet(liquidPlaneSig))
	{
		ECLiquidPlane& plane = entity.GetComponent<ECLiquidPlane>();
		plane.SetShouldGenerateMesh(true);
		plane.MaybeUpdate(entity, world);
		
		if (plane.NumIndices() != 0)
		{
			float distToCamera = std::abs(entity.GetComponent<eg::ECPosition3D>().position.y - cameraPos.y);
			m_planes.emplace_back(distToCamera, &entity.GetComponent<ECLiquidPlane>());
		}
	}
	
	std::sort(m_planes.begin(), m_planes.end());
}

void LiquidPlaneRenderer::Render() const
{
	if (m_planes.empty())
		return;
	
	eg::DC.BindPipeline(m_pipeline);
	
	eg::DC.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
	
	for (int64_t i = (int64_t)m_planes.size() - 1; i >= 0; i--)
	{
		const ECLiquidPlane& plane = *m_planes[i].second;
		
		eg::DC.BindVertexBuffer(0, plane.VertexBuffer(), 0);
		eg::DC.BindIndexBuffer(eg::IndexType::UInt16, plane.IndexBuffer(), 0);
		
		eg::DC.PushConstants(0, sizeof(float) * 4, &plane.EditorColor());
		
		eg::DC.DrawIndexed(0, plane.NumIndices(), 0, 0, 1);
	}
}
