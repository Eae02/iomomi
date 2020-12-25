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
	m_pipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_HDR, GB_DEPTH_FORMAT);
	m_pipeline.FramebufferFormatHint(LIGHT_COLOR_FORMAT_LDR, GB_DEPTH_FORMAT);
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
	if (mDrawArgs->drawMode != MeshDrawMode::TransparentFinal)
		return false;
	
	cmdCtx.BindPipeline(m_pipeline);
	cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
	cmdCtx.PushConstants(0, sizeof(m_intensityBoost), &m_intensityBoost);
	return true;
}

bool GravityGun::MidMaterial::BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const
{
	return true;
}

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
	
	for (size_t i = 0; i < m_model->NumMeshes(); i++)
	{
		std::string_view name = m_model->GetMesh(i).name;
		m_meshIsPartOfFront[i] = (eg::StringStartsWith(name, "Front") || eg::StringStartsWith(name, "EnergyCyl"));
	}
	
	light = std::make_shared<PointLight>();
	light->enabled = false;
	light->castsShadows = true;
	light->willMoveEveryFrame = true;
}

static glm::vec3 GUN_POSITION(2.44f, -2.16f, -6.5f);
static float GUN_ROTATION_Y = -1.02f;
static float GUN_ROTATION_X = -0.02f;
static float GUN_SCALE = 0.04f;
static float GUN_BOB_SCALE = 0.3f;
static float GUN_BOB_SPEED = 4.0f;

static float ORB_SPEED = 60.0f;

static float LIGHT_INTENSITY_MAX = 20.0f;
static float LIGHT_INTENSITY_FALL_TIME = 0.2f;
static eg::ColorSRGB LIGHT_COLOR = eg::ColorSRGB::FromHex(0xb6fdff);

void GravityGun::SetBeamInstanceTransform(BeamInstance& instance)
{
	instance.particleEmitter.SetTransform(glm::translate(glm::mat4(1), instance.beamPos) * glm::mat4(instance.rotationMatrix));
}

void GravityGun::Update(World& world, const PhysicsEngine& physicsEngine, WaterSimulator& waterSim,
	eg::ParticleManager& particleManager, const Player& player, const glm::mat4& inverseViewProj, float dt)
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
	
	m_fireAnimationTime = std::max(m_fireAnimationTime - dt, 0.0f);
	m_midMaterial.m_intensityBoost = glm::smoothstep(0.0f, 1.0f, m_fireAnimationTime) * 1;
	
	//Updates beam instances
	BeamInstance* lightBeamInstance = nullptr;
	for (BeamInstance& beamInstance : m_beamInstances)
	{
		beamInstance.timeRemaining -= dt;
		if (beamInstance.timeRemaining < 0)
		{
			beamInstance.beamPos = beamInstance.targetPos;
			if (std::shared_ptr<EntGravityChargeable> targetEntity = beamInstance.entityToCharge.lock())
			{
				targetEntity->SetGravity(beamInstance.newDown);
			}
			
			beamInstance.lightIntensity -= dt / LIGHT_INTENSITY_FALL_TIME;
			beamInstance.particleEmitter.Kill();
		}
		else
		{
			beamInstance.beamPos += beamInstance.direction * dt;
			SetBeamInstanceTransform(beamInstance);
		}
		
		if (lightBeamInstance == nullptr || beamInstance.timeRemaining < lightBeamInstance->timeRemaining)
		{
			lightBeamInstance = &beamInstance;
		}
	}
	
	light->enabled = settings.gunFlash && lightBeamInstance != nullptr;
	if (lightBeamInstance != nullptr)
	{
		light->position = lightBeamInstance->beamPos;
		light->SetRadiance(LIGHT_COLOR, lightBeamInstance->lightIntensity * LIGHT_INTENSITY_MAX);
	}
	
	//Removes dead beam instances
	for (long i = (long)m_beamInstances.size() - 1; i >= 0; i--)
	{
		if (m_beamInstances[i].lightIntensity < 0)
		{
			if (i != (long)m_beamInstances.size() - 1)
			{
				m_beamInstances.back() = std::move(m_beamInstances[i]);
			}
			m_beamInstances.pop_back();
		}
	}
	
	eg::Ray viewRay = eg::Ray::UnprojectNDC(inverseViewProj, glm::vec2(0.0f));
	auto[intersectObject, intersectDist] = physicsEngine.RayIntersect(viewRay, RAY_MASK_BLOCK_GUN);
	
	shouldShowControlHint = false;
	std::shared_ptr<EntGravityChargeable> entGravityChargeable;
	if (intersectObject && std::holds_alternative<Ent*>(intersectObject->owner))
	{
		entGravityChargeable = std::dynamic_pointer_cast<EntGravityChargeable>(std::get<Ent*>(intersectObject->owner)->shared_from_this());
		if (entGravityChargeable)
		{
			shouldShowControlHint = entGravityChargeable->ShouldShowGravityBeamControlHint(player.CurrentDown());
		}
	}
	
	if (settings.keyShoot.IsDown() && !settings.keyShoot.WasDown())
	{
		auto[waterIntersectDst, waterIntersectParticle] = waterSim.RayIntersect(viewRay);
		if (intersectObject || waterIntersectParticle != -1)
		{
			BeamInstance& beamInstance = m_beamInstances.emplace_back();
			
			beamInstance.particleEmitter = particleManager.AddEmitter(eg::GetAsset<eg::ParticleEmitterType>("Particles/BlueOrb.ype"));
			
			glm::vec3 start = m_gunTransform * glm::vec4(0, 0, 0, 1);
			glm::vec3 target = viewRay.GetPoint(intersectDist * 0.99f);
			
			beamInstance.newDown = player.CurrentDown();
			beamInstance.direction = glm::normalize(target - start) * ORB_SPEED;
			beamInstance.targetPos = target;
			beamInstance.timeRemaining = intersectDist / ORB_SPEED;
			beamInstance.beamPos = start;
			beamInstance.lightIntensity = 1;
			beamInstance.lightInstanceID = LightSource::NextInstanceID();
			
			glm::vec3 rotationL = glm::normalize(glm::cross(beamInstance.direction, glm::vec3(0, 1, 0)));
			glm::vec3 rotationU = glm::normalize(glm::cross(beamInstance.direction, rotationL));
			beamInstance.rotationMatrix = glm::mat3(rotationL, rotationU, glm::normalize(beamInstance.direction));
			
			SetBeamInstanceTransform(beamInstance);
			
			m_fireAnimationTime = 1;
			
			if (waterIntersectParticle != -1 && waterIntersectDst < intersectDist)
			{
				waterSim.ChangeGravity(waterIntersectParticle, player.CurrentDown());
			}
			else
			{
				beamInstance.entityToCharge = entGravityChargeable;
			}
		}
	}
}

void GravityGun::Draw(eg::MeshBatch& meshBatch, eg::MeshBatchOrdered& transparentMeshBatch)
{
	constexpr float KICK_BACK_TIME = 0.08f;
	constexpr float KICK_RESTORE_TIME = 0.7f;
	constexpr float KICK_BACK_DIST_MAX = 0.3f;
	constexpr float KICK_BACK_DIST_MAX_FRONT = 0.5f;
	constexpr float ANIMATION_END = 1 - KICK_BACK_TIME - KICK_RESTORE_TIME;
	float kickBackDist = 0;
	if (m_fireAnimationTime > 1 - KICK_BACK_TIME)
	{
		kickBackDist = glm::smoothstep(0.0f, 1.0f, (1 - m_fireAnimationTime) / KICK_BACK_TIME);
	}
	else if (m_fireAnimationTime > ANIMATION_END)
	{
		kickBackDist = glm::smoothstep(0.0f, 1.0f, (m_fireAnimationTime - ANIMATION_END) / (KICK_RESTORE_TIME));
	}
	
	for (size_t m = 0; m < m_model->NumMeshes(); m++)
	{
		glm::mat4 transform = m_gunTransform;
		float dstForThisMesh = m_meshIsPartOfFront[m] ? KICK_BACK_DIST_MAX_FRONT : KICK_BACK_DIST_MAX;
		transform = glm::translate(transform, glm::vec3(0, 0, -kickBackDist * dstForThisMesh));
		
		StaticPropMaterial::InstanceData instanceData(transform);
		
		const eg::IMaterial* material = m_materials[m_model->GetMesh(m).materialIndex];
		if (material->GetOrderRequirement() == eg::IMaterial::OrderRequirement::OnlyOrdered)
		{
			transparentMeshBatch.AddModelMesh(*m_model, m, *material, instanceData, INFINITY);
		}
		else
		{
			meshBatch.AddModelMesh(*m_model, m, *material, instanceData, -1);
		}
	}
}
