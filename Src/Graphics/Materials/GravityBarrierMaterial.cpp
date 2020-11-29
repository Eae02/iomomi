#include "GravityBarrierMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../GraphicsCommon.hpp"
#include "../RenderSettings.hpp"

GravityBarrierMaterial::GravityBarrierMaterial()
{
	const auto& fs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier.fs.glsl");
	
	eg::SpecializationConstantEntry waterModeSpecConstant;
	waterModeSpecConstant.constantID = WATER_MODE_CONST_ID;
	waterModeSpecConstant.size = sizeof(int32_t);
	waterModeSpecConstant.offset = 0;
	
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = fs.GetVariant("VGame");
	pipelineCI.vertexBindings[0] = { 2, eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::UInt8Norm, 2, 0 };
	pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 3, offsetof(InstanceData, position) };
	pipelineCI.vertexAttributes[2] = { 1, eg::DataType::UInt8, 2, offsetof(InstanceData, opacity) };
	pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(InstanceData, tangent) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(InstanceData, bitangent) };
	pipelineCI.topology = eg::Topology::TriangleStrip;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.enableDepthTest = true;
	pipelineCI.enableDepthWrite = false;
	pipelineCI.depthCompare = eg::CompareOp::Less;
	pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
	pipelineCI.fragmentShader.specConstants = { &waterModeSpecConstant, 1 };
	pipelineCI.fragmentShader.specConstantsDataSize = sizeof(int32_t);
	
	pipelineCI.label = "GravityBarrier[BeforeWater]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_BEFORE);
	m_pipelineGameBeforeWater = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.label = "GravityBarrier[Final]";
	pipelineCI.fragmentShader.specConstantsData = const_cast<int32_t*>(&WATER_MODE_AFTER);
	m_pipelineGameFinal = eg::Pipeline::Create(pipelineCI);
	
	pipelineCI.label = "GravityBarrier[Editor]";
	pipelineCI.fragmentShader = fs.GetVariant("VEditor");
	m_pipelineEditor = eg::Pipeline::Create(pipelineCI);
	
	m_barrierDataBuffer = eg::Buffer(eg::BufferFlags::Update | eg::BufferFlags::UniformBuffer,
		sizeof(BarrierBufferData), nullptr);
}

size_t GravityBarrierMaterial::PipelineHash() const
{
	return typeid(GravityBarrierMaterial).hash_code();
}

bool GravityBarrierMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);
	
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeWater)
	{
		cmdCtx.BindPipeline(m_pipelineGameBeforeWater);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 0, 1);
		cmdCtx.BindUniformBuffer(m_barrierDataBuffer, 0, 2, 0, sizeof(BarrierBufferData));
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 0, 3);
		cmdCtx.BindTexture(blackPixelTexture, 0, 4);
		return true;
	}
	if (mDrawArgs->drawMode == MeshDrawMode::TransparentBeforeBlur)
	{
		cmdCtx.BindPipeline(m_pipelineGameFinal);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 0, 1);
		cmdCtx.BindUniformBuffer(m_barrierDataBuffer, 0, 2, 0, sizeof(BarrierBufferData));
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 0, 3);
		cmdCtx.BindTexture(mDrawArgs->rtManager->GetRenderTexture(RenderTex::BlurredGlassDepth), 0, 4);
		return true;
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::TransparentFinal)
	{
		cmdCtx.BindPipeline(m_pipelineGameFinal);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 0, 1);
		cmdCtx.BindUniformBuffer(m_barrierDataBuffer, 0, 2, 0, sizeof(BarrierBufferData));
		cmdCtx.BindTexture(mDrawArgs->waterDepthTexture, 0, 3);
		cmdCtx.BindTexture(blackPixelTexture, 0, 4);
		return true;
	}
	else if (mDrawArgs->drawMode == MeshDrawMode::Editor)
	{
		cmdCtx.BindPipeline(m_pipelineEditor);
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		cmdCtx.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 0, 1);
		return true;
	}
	
	return false;
}

bool GravityBarrierMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}
