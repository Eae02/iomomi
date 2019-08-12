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
	pipelineCI.vertexBindings[1] = { sizeof(float) * 16, eg::InputRate::Instance };
	pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, offsetof(eg::StdVertex, position) };
	pipelineCI.vertexAttributes[1] = { 0, eg::DataType::Float32, 2, offsetof(eg::StdVertex, texCoord) };
	pipelineCI.vertexAttributes[2] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, normal) };
	pipelineCI.vertexAttributes[3] = { 0, eg::DataType::SInt8Norm, 3, offsetof(eg::StdVertex, tangent) };
	pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, 0 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[5] = { 1, eg::DataType::Float32, 4, 1 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[6] = { 1, eg::DataType::Float32, 4, 2 * sizeof(float) * 4 };
	pipelineCI.vertexAttributes[7] = { 1, eg::DataType::Float32, 4, 3 * sizeof(float) * 4 };
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
	eg::EntityHandle entityToCharge;
	float lightIntensity;
};

static eg::EntitySignature gravityOrbSignature =
	eg::EntitySignature::Create<ECGravityOrb, eg::ECPosition3D, eg::ECParticleSystem, PointLight>();

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
	
	for (eg::Entity& orbEntity : world.EntityManager().GetEntitySet(gravityOrbSignature))
	{
		ECGravityOrb& orbComp = orbEntity.GetComponent<ECGravityOrb>();
		
		orbComp.timeAlive -= dt;
		
		if (orbComp.timeAlive < 0)
		{
			orbEntity.GetComponent<eg::ECPosition3D>().position = orbComp.targetPos;
			if (eg::Entity* targetEntity = orbComp.entityToCharge.Get())
			{
				bool set = false;
				
				GravityChargeSetMessage chargeSetMessage;
				chargeSetMessage.newDown = orbComp.newDown;
				chargeSetMessage.set = &set;
				
				targetEntity->HandleMessage(chargeSetMessage);
				
				orbComp.entityToCharge = { };
				
				orbEntity.GetComponent<eg::ECParticleSystem>().ClearEmitters();
			}
			
			orbEntity.GetComponent<PointLight>().SetRadiance(LIGHT_COLOR, orbComp.lightIntensity);
			orbComp.lightIntensity -= LIGHT_INTENSITY_FALL_SPEED * dt;
			
			if (orbComp.lightIntensity < 0)
			{
				orbEntity.Despawn();
			}
		}
		else
		{
			orbEntity.GetComponent<eg::ECPosition3D>().position += orbComp.direction * dt;
		}
	}
	
	if ((eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft)) || 
		(eg::IsButtonDown(eg::Button::CtrlrRightShoulder) && !eg::WasButtonDown(eg::Button::CtrlrRightShoulder)) ||
		(eg::IsButtonDown(eg::Button::CtrlrLeftShoulder) && !eg::WasButtonDown(eg::Button::CtrlrLeftShoulder)))
	{
		eg::Ray viewRay = eg::Ray::UnprojectNDC(inverseViewProj, glm::vec2(0.0f));
		RayIntersectResult intersectResult = world.RayIntersect(viewRay);
		
		auto [waterIntersectDst, waterIntersectParticle] = waterSim.RayIntersect(viewRay);
		
		if (intersectResult.intersected || waterIntersectParticle != -1)
		{
			eg::Entity& orbEntity = world.EntityManager().AddEntity(gravityOrbSignature);
			orbEntity.InitComponent<eg::ECParticleSystem>(&particleManager).AddEmitter(
				eg::GetAsset<eg::ParticleEmitterType>("Particles/BlueOrb.ype"));
			
			glm::vec3 start = m_gunTransform * glm::vec4(0, 0, 0, 1);
			glm::vec3 target = viewRay.GetPoint(intersectResult.distance * 0.99f);
			
			PointLight& light = orbEntity.GetComponent<PointLight>();
			light.SetRadiance(LIGHT_COLOR, 15.0f);
			light.castsShadows = settings.shadowQuality >= QualityLevel::Medium;
			
			ECGravityOrb& orbComp = orbEntity.GetComponent<ECGravityOrb>();
			orbComp.newDown = player.CurrentDown();
			orbComp.direction = glm::normalize(target - start) * ORB_SPEED;
			orbComp.targetPos = target;
			orbComp.timeAlive = intersectResult.distance / ORB_SPEED;
			orbComp.lightIntensity = 15.0f;
			
			if (waterIntersectParticle != -1 && waterIntersectDst < intersectResult.distance)
			{
				waterSim.ChangeGravity(waterIntersectParticle, player.CurrentDown());
			}
			else if (intersectResult.entity != nullptr)
			{
				orbComp.entityToCharge = *intersectResult.entity;
			}
			
			orbEntity.InitComponent<eg::ECPosition3D>(start);
		}
	}
}

void GravityGun::Draw(eg::MeshBatch& meshBatch)
{
	for (size_t m = 0; m < m_model->NumMeshes(); m++)
	{
		meshBatch.AddModelMesh(*m_model, m, *m_materials[m_model->GetMesh(m).materialIndex], m_gunTransform, -1);
	}
}
