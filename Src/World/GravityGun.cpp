#include "GravityGun.hpp"
#include "World.hpp"
#include "Player.hpp"
#include "../Graphics/DeferredRenderer.hpp"
#include "../Graphics/Materials/StaticPropMaterial.hpp"
#include "../Graphics/Materials/MeshDrawArgs.hpp"
#include "../Settings.hpp"
#include "../Graphics/RenderSettings.hpp"
#include "../Graphics/WaterSimulator.hpp"

#include <imgui.h>

GravityGun::MidMaterial::MidMaterial()
{
	eg::GraphicsPipelineCreateInfo pipelineCI;
	pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/Common3D.vs.glsl").DefaultVariant();
	pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityGunMid.fs.glsl").DefaultVariant();
	pipelineCI.enableDepthWrite = false;
	pipelineCI.enableDepthTest = true;
	pipelineCI.cullMode = eg::CullMode::None;
	pipelineCI.vertexBindings[0] = { sizeof(eg::StdVertex), eg::InputRate::Vertex };
	pipelineCI.vertexBindings[1] = { sizeof(StaticPropMaterial::InstanceData), eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, normal) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, tangent) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 0 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 16 };
	pipelineCI.vertexAttributes[6] = { 1, eg::DataType::Float32, 4, offsetof(StaticPropMaterial::InstanceData, transform) + 32 };
	pipelineCI.vertexAttributes[7] = { 1, eg::DataType::Float32, 2, offsetof(StaticPropMaterial::InstanceData, textureScale) };
	pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
	pipelineCI.label = "GravityGunMid";
	m_pipeline = eg::Pipeline::Create(pipelineCI);
	m_pipeline.FramebufferFormatHint(DeferredRenderer::LIGHT_COLOR_FORMAT_HDR, DeferredRenderer::DEPTH_FORMAT);
	m_pipeline.FramebufferFormatHint(DeferredRenderer::LIGHT_COLOR_FORMAT_LDR, DeferredRenderer::DEPTH_FORMAT);
	m_pipeline.FramebufferFormatHint(eg::Format::DefaultColor, eg::Format::DefaultDepthStencil);
	
	m_descriptorSet = eg::DescriptorSet(m_pipeline, 0);
	
	m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
	m_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/Hex.png"), 1);
}

size_t GravityGun::MidMaterial::PipelineHash() const
{
	return typeid(GravityGun::MidMaterial).hash_code();
}

bool GravityGun::MidMaterial::BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);
	if (mDrawArgs->drawMode != MeshDrawMode::Emissive)
		return false;
	
	cmdCtx.BindPipeline(m_pipeline);
	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
	return true;
}

bool GravityGun::MidMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}

struct ECGravityOrb
{
	glm::vec3 direction;
	glm::vec3 targetPos;
	float timeAlive;
	Dir newDown;
	float lightIntensity;
};

GravityGun::GravityGun()
{
	m_model = &eg::GetAsset<eg::Model>("Models/GravityGun.obj");
	if (m_model->NumMaterials() > m_materials.size())
	{
		EG_PANIC("Too many materials in gravity gun model");
	}
	
	std::fill(m_materials.begin(), m_materials.end(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"));
	
	m_materials.at(m_model->GetMaterialIndex("EnergyCyl")) = &m_midMaterial;
	m_materials.at(m_model->GetMaterialIndex("Main")) = &eg::GetAsset<StaticPropMaterial>("Materials/GravityGunMain.yaml");
}

static glm::vec3 GUN_POSITION(2.44f, -2.16f, -6.5f);
static float GUN_ROTATION_Y = -1.02f;
static float GUN_ROTATION_X = -0.02f;
static float GUN_SCALE = 0.04f;
static float GUN_BOB_SCALE = 0.3f;
static float GUN_BOB_SPEED = 4.0f;

static float ORB_SPEED = 25.0f;

static eg::ColorSRGB LIGHT_COLOR = eg::ColorSRGB::FromHex(0x19ebd8);
static float LIGHT_INTENSITY_FALL_SPEED = 30.0f;

void GravityGun::Update(World& world, WaterSimulator& waterSim, eg::ParticleManager& particleManager,
	const Player& player, const glm::mat4& inverseViewProj, float dt)
{
	glm::mat3 rotationMatrix = (glm::mat3_cast(player.Rotation()));
	
	//Uncomment this to add an imgui window for customizing weapon location
	/*
	if (ImGui::Begin("Weapon"))
	{
		ImGui::DragFloat3("Position", &GUN_POSITION.x, 0.1f);
		ImGui::DragFloat("RotY", &GUN_ROTATION_Y, 0.001f);
		ImGui::DragFloat("RotX", &GUN_ROTATION_X, 0.001f);
		ImGui::DragFloat("Scale", &GUN_SCALE, 0.001f, 0.0f, 1.0f);
		ImGui::DragFloat("Bob Scale", &GUN_BOB_SCALE, 0.001f, 0.0f, 1.0f);
	}
	ImGui::End();
	*/
	
	float fovOffsetZ = 0.07f * (settings.fieldOfViewDeg - 80.0f);
	glm::vec3 targetOffset = rotationMatrix * ((GUN_POSITION + glm::vec3(0, 0, fovOffsetZ)) * GUN_SCALE);
	m_gunOffset += (targetOffset - m_gunOffset) * std::min(dt * 30.0f, 1.0f);
	
	glm::vec3 planeVel = player.Velocity();
	glm::vec3 upAxis = DirectionVector(player.CurrentDown());
	planeVel -= glm::dot(planeVel, upAxis) * upAxis;
	
	float speed = glm::length(planeVel);
	float xoffset = sin(m_bobTime) * speed * GUN_BOB_SCALE / 100;
	float yoffset = sin(2 * m_bobTime) * speed * GUN_BOB_SCALE / 400;
	m_bobTime += dt * GUN_BOB_SPEED;
	
	glm::vec3 translation = (rotationMatrix * glm::vec3(xoffset, yoffset, 0)) + player.EyePosition() + m_gunOffset;
	
	m_gunTransform = glm::translate(glm::mat4(), translation) * glm::mat4(rotationMatrix);
	m_gunTransform = glm::rotate(m_gunTransform, eg::PI * GUN_ROTATION_X, glm::vec3(1, 0, 0));
	m_gunTransform = glm::rotate(m_gunTransform, eg::PI * GUN_ROTATION_Y, glm::vec3(0, 1, 0));
	m_gunTransform = glm::scale(m_gunTransform, glm::vec3(GUN_SCALE));
	
	if (m_beamTimeRemaining > 0)
	{
		m_beamTimeRemaining -= dt;
		if (m_beamTimeRemaining < 0)
		{
			m_beamPos = m_beamTargetPos;
			if (std::shared_ptr<EntGravityChargeable> targetEntity = m_entityToCharge.lock())
			{
				targetEntity->SetGravity(m_newDown);
				m_orbParticleEmitter.Kill();
			}
			
			//orbEntity.GetComponent<PointLight>().SetRadiance(LIGHT_COLOR, orbComp.lightIntensity);
			//orbComp.lightIntensity -= LIGHT_INTENSITY_FALL_SPEED * dt;
			
			//if (orbComp.lightIntensity < 0)
			//{
			//	orbEntity.Despawn();
			//}
		}
		else
		{
			m_beamPos += m_beamDirection * dt;
			m_orbParticleEmitter.SetTransform(glm::translate(glm::mat4(1), m_beamPos));
		}
	}
	
	if ((eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft)) ||
		(eg::IsButtonDown(eg::Button::CtrlrRightShoulder) && !eg::WasButtonDown(eg::Button::CtrlrRightShoulder)) ||
		(eg::IsButtonDown(eg::Button::CtrlrLeftShoulder) && !eg::WasButtonDown(eg::Button::CtrlrLeftShoulder)))
	{
		eg::Ray viewRay = eg::Ray::UnprojectNDC(inverseViewProj, glm::vec2(0.0f));
		RayIntersectResult intersectResult = world.RayIntersect(viewRay);
		
		auto[waterIntersectDst, waterIntersectParticle] = waterSim.RayIntersect(viewRay);
		
		if (intersectResult.intersected || waterIntersectParticle != -1)
		{
			m_orbParticleEmitter = particleManager.AddEmitter(eg::GetAsset<eg::ParticleEmitterType>("Particles/BlueOrb.ype"));
			
			glm::vec3 start = m_gunTransform * glm::vec4(0, 0, 0, 1);
			glm::vec3 target = viewRay.GetPoint(intersectResult.distance * 0.99f);
			
			//PointLight& light = orbEntity.GetComponent<PointLight>();
			//light.SetRadiance(LIGHT_COLOR, 15.0f);
			//light.castsShadows = settings.shadowQuality >= QualityLevel::Medium;
			
			m_newDown = player.CurrentDown();
			m_beamDirection = glm::normalize(target - start) * ORB_SPEED;
			m_beamTargetPos = target;
			m_beamTimeRemaining = intersectResult.distance / ORB_SPEED;
			m_beamPos = start;
			//orbComp.lightIntensity = 15.0f;
			
			m_entityToCharge = { };
			if (waterIntersectParticle != -1 && waterIntersectDst < intersectResult.distance)
			{
				waterSim.ChangeGravity(waterIntersectParticle, player.CurrentDown());
			}
			else if (intersectResult.entity != nullptr)
			{
				m_entityToCharge = std::dynamic_pointer_cast<EntGravityChargeable>(intersectResult.entity->shared_from_this());
			}
		}
	}
}

void GravityGun::Draw(eg::MeshBatch& meshBatch)
{
	for (size_t m = 0; m < m_model->NumMeshes(); m++)
	{
		meshBatch.AddModelMesh(*m_model, m, *m_materials[m_model->GetMesh(m).materialIndex],
			StaticPropMaterial::InstanceData(m_gunTransform), -1);
	}
}
