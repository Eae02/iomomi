#include "PumpScreenMaterial.hpp"
#include "MeshDrawArgs.hpp"
#include "../RenderSettings.hpp"
#include "../../Game.hpp"

static eg::Pipeline pipelineGame;
static const eg::Texture* arrowTexture;
static eg::Sampler arrowTextureSampler;

static void OnInit()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	StaticPropMaterial::InitializeForCommon3DVS(pipelineCI);
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/PumpScreenMaterial.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = true;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::Back;
	pipelineCI.numColorAttachments = 1;
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.label = "PumpScreenGame";
	pipelineGame = eg::Pipeline::Create(pipelineCI);
	
	arrowTexture = &eg::GetAsset<eg::Texture>("Textures/PumpDisplayArrow.png");
	
	eg::SamplerDescription samplerDesc;
	samplerDesc.wrapU = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapV = eg::WrapMode::ClampToEdge;
	samplerDesc.wrapW = eg::WrapMode::ClampToEdge;
	arrowTextureSampler = eg::Sampler(samplerDesc);
}

static void OnShutdown()
{
	pipelineGame.Destroy();
	arrowTextureSampler = {};
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

static constexpr int NUM_ARROWS = 8;

struct PumpMaterialData
{
	eg::ColorLin color;
	glm::vec4 arrowRects[NUM_ARROWS];
	float arrowOpacities[NUM_ARROWS];
};

PumpScreenMaterial::PumpScreenMaterial()
{
	m_descriptorSet = eg::DescriptorSet(pipelineGame, 0);
	
	m_dataBuffer = eg::Buffer(eg::BufferFlags::UniformBuffer | eg::BufferFlags::Update, sizeof(PumpMaterialData), nullptr);
	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	m_descriptorSet.BindUniformBuffer(m_dataBuffer, 1, 0, sizeof(PumpMaterialData));
	m_descriptorSet.BindTexture(*arrowTexture, 2, &arrowTextureSampler);
	
	Update(0, 0);
}

constexpr float SCREEN_AR = 0.319;

static float* pumpAnimationSpeed = eg::TweakVarFloat("pump_anim_speed", 3.0f);
static float* pumpAnimationGradient = eg::TweakVarFloat("pump_anim_grad", 1.3f);
static float* pumpAnimationFadeS = eg::TweakVarFloat("pump_anim_fade_s", 0.05f);
static float* pumpAnimationFadeE = eg::TweakVarFloat("pump_anim_fade_e", 0.2f);
static float* pumpDirFadeTime = eg::TweakVarFloat("pump_dir_fade_time", 0.1f);

void PumpScreenMaterial::Update(float dt, int direction)
{
	if (m_currentDirection != direction)
	{
		m_opacity -= dt / *pumpDirFadeTime;
		if (m_opacity <= 0 || m_currentDirection == 0)
		{
			m_opacity = 0;
			m_currentDirection = direction;
		}
	}
	else if (m_currentDirection == direction && m_opacity < 1)
	{
		m_opacity = std::min(m_opacity + dt / *pumpDirFadeTime, 1.0f);
	}
	
	m_animationProgress = std::fmod(m_animationProgress + dt * *pumpAnimationSpeed, 1.0f);
	
	PumpMaterialData materialData = {};
	materialData.color = eg::ColorLin(eg::ColorSRGB::FromHex(0xa7dde6));
	
	const float arrowHeightTS = 0.75f;
	const float arrowWidthTS = arrowHeightTS * ((float)arrowTexture->Width() / (float)arrowTexture->Height()) * SCREEN_AR;
	const float minX = -arrowWidthTS;
	const float maxX = 1;
	
	if (m_currentDirection != 0)
	{
		for (int arrow = 0; arrow < NUM_ARROWS; arrow++)
		{
			float a = (arrow + m_animationProgress) / (float)(NUM_ARROWS);
			if (m_currentDirection == -1)
				a = 1 - a;
			
			float a2 = a * a;
			a = (2 * *pumpAnimationGradient - 2) * a * a2 + (-3 * *pumpAnimationGradient + 3) * a2 + *pumpAnimationGradient * a;
			
			materialData.arrowRects[arrow] = glm::vec4(
				glm::mix(minX, maxX, a),
				(1.0 - arrowHeightTS) / 2.0f,
				1.0f / arrowWidthTS,
				1.0f / arrowHeightTS
			);
			
			if (m_currentDirection == -1)
			{
				materialData.arrowRects[arrow].x += arrowWidthTS;
				materialData.arrowRects[arrow].z *= -1;
			}
			
			float opacity = (std::min(a, 1 - a) - *pumpAnimationFadeS) / (*pumpAnimationFadeE - *pumpAnimationFadeS);
			materialData.arrowOpacities[arrow] = glm::clamp(opacity, 0.0f, 1.0f) * m_opacity;
		}
	}
	
	eg::DC.UpdateBuffer(m_dataBuffer, 0, sizeof(PumpMaterialData), &materialData);
	m_dataBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
}

size_t PumpScreenMaterial::PipelineHash() const
{
	return typeid(PumpScreenMaterial).hash_code();
}

bool PumpScreenMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = static_cast<MeshDrawArgs*>(drawArgs);
	
	if (mDrawArgs->drawMode != MeshDrawMode::Emissive && mDrawArgs->drawMode != MeshDrawMode::Editor)
		return false;
	
	cmdCtx.BindPipeline(pipelineGame);
	
	return true;
}

bool PumpScreenMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
	
	return true;
}
