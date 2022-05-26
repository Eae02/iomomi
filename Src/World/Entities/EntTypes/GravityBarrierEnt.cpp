#include "GravityBarrierEnt.hpp"
#include "Activation/CubeEnt.hpp"
#include "../../World.hpp"
#include "../../Player.hpp"
#include "../../../Graphics/RenderSettings.hpp"
#include "../../../Graphics/Materials/MeshDrawArgs.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../ImGui.hpp"
#include "../../../../Protobuf/Build/GravityBarrierEntity.pb.h"

#include <pcg_random.hpp>

static eg::Buffer vertexBuffer;
static eg::MeshBatch::Mesh barrierMesh;

static eg::Model* barrierBorderModel;
static size_t meshIndexBody;
static size_t meshIndexEmissive;

static eg::IMaterial* borderMaterial;

static void OnInit()
{
	static const uint8_t vertices[] = { 0, 0, 255, 0, 0, 255, 255, 255 };
	vertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(vertices), vertices);
	vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	
	barrierMesh.firstIndex = 0;
	barrierMesh.firstVertex = 0;
	barrierMesh.numElements = 4;
	barrierMesh.vertexBuffer = vertexBuffer;
	
	barrierBorderModel = &eg::GetAsset<eg::Model>("Models/GravityBarrierBorder.obj");
	meshIndexBody = barrierBorderModel->GetMeshIndex("Body");
	meshIndexEmissive = barrierBorderModel->GetMeshIndex("Lights");
	
	borderMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/GravityBarrierBorder.yaml");
}

static void OnShutdown()
{
	vertexBuffer.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

extern pcg32_fast globalRNG;

GravityBarrierEnt::GravityBarrierEnt()
	: m_activatable(&GravityBarrierEnt::GetConnectionPoints)
{
	m_physicsObject.rayIntersectMask = RAY_MASK_BLOCK_PICK_UP;
	m_physicsObject.canBePushed = false;
	m_physicsObject.owner = this;
	m_physicsObject.shouldCollide = &GravityBarrierEnt::ShouldCollide;
	m_physicsObject.debugColor = 0xcf24cf;
	m_material.noiseSampleOffset = std::uniform_real_distribution<float>(0.0f, 1.0f)(globalRNG);
}

bool GravityBarrierEnt::ShouldCollide(const PhysicsObject& self, const PhysicsObject& other)
{
	GravityBarrierEnt& selfBarrier = *(GravityBarrierEnt*)std::get<Ent*>(self.owner);
	if (!selfBarrier.m_enabled)
		return false;
	
	Dir otherDown = Dir::PosX;
	if (auto player = std::get_if<Player*>(&other.owner))
	{
		otherDown = (**player).CurrentDown();
	}
	else if (auto otherEntDP = std::get_if<Ent*>(&other.owner))
	{
		const Ent& otherEnt = **otherEntDP;
		if (otherEnt.TypeID() == EntTypeID::Cube)
		{
			otherDown = ((const CubeEnt&)otherEnt).CurrentDown();
		}
		else
		{
			return false;
		}
	}
	if ((int)otherDown / 2 == selfBarrier.BlockedAxis())
		return true;
	if (selfBarrier.m_blockFalling && (int)otherDown / 2 == (int)selfBarrier.m_aaQuad.upPlane / 2)
		return true;
	return false;
}

std::vector<glm::vec3> GravityBarrierEnt::GetConnectionPoints(const Ent& entity)
{
	const GravityBarrierEnt& barrierEnt = (const GravityBarrierEnt&)entity;
	auto [tangent, bitangent] = barrierEnt.GetTangents();
	return {
		barrierEnt.m_position + bitangent * 0.5f,
		barrierEnt.m_position - bitangent * 0.5f,
		barrierEnt.m_position + tangent * 0.5f,
		barrierEnt.m_position - tangent * 0.5f
	};
}

glm::mat4 GravityBarrierEnt::GetTransform() const
{
	auto [tangent, bitangent] = GetTangents();
	return glm::mat4(
		glm::vec4(tangent, 0),
		glm::vec4(bitangent, 0),
		glm::vec4(glm::normalize(glm::cross(tangent, bitangent)), 0),
		glm::vec4(m_position, 1));
}

static const glm::vec4 gravityBarrierLightColor = glm::vec4(0.1, 0.7, 0.9, 0.0f);
constexpr float LIGHT_INTENSITY_MIN = 1.0f;
constexpr float LIGHT_INTENSITY_MAX = 5.0f;

void GravityBarrierEnt::CommonDraw(const EntDrawArgs& args)
{
	auto [tangent, bitangent] = GetTangents();
	
	float tangentLen = glm::length(tangent);
	float bitangentLen = glm::length(bitangent);
	
	eg::AABB wholeAABB = std::get<eg::AABB>(m_physicsObject.shape);
	wholeAABB.min += m_position;
	wholeAABB.max += m_position;
	if (!args.frustum->Intersects(wholeAABB))
		return;
	
	if (args.transparentMeshBatch)
	{
		m_material.position = m_position;
		m_material.opacity = glm::smoothstep(0.0f, 1.0f, m_opacity);
		m_material.blockedAxis = BlockedAxis();
		m_material.tangent = tangent;
		m_material.bitangent = bitangent;
		m_material.waterDistanceTexture = waterDistanceTexture;
		
		args.transparentMeshBatch->AddNoData(barrierMesh, m_material, DepthDrawOrder(m_position));
	}
	
	glm::vec3 tangentNorm = tangent / tangentLen;
	glm::vec3 bitangentNorm = bitangent / bitangentLen;
	glm::vec3 tcb = glm::cross(tangentNorm, bitangentNorm);
	
	auto AddBorderModelInstance = [&] (const glm::mat4& transform, float lightIntensity)
	{
		eg::AABB aabb = barrierBorderModel->GetMesh(meshIndexBody).boundingAABB->TransformedBoundingBox(transform);
		if (!args.frustum->Intersects(aabb))
			return;
		
		args.meshBatch->AddModelMesh(*barrierBorderModel, meshIndexBody, *borderMaterial,
		                             StaticPropMaterial::InstanceData(transform));
		
		const float intensity = glm::mix(LIGHT_INTENSITY_MIN, LIGHT_INTENSITY_MAX, lightIntensity);
		args.meshBatch->AddModelMesh(*barrierBorderModel, meshIndexEmissive, EmissiveMaterial::instance,
		                             EmissiveMaterial::InstanceData { transform, gravityBarrierLightColor * intensity });
	};
	
	int tangentLenI = (int)std::round(tangentLen);
	int bitangentLenI = (int)std::round(bitangentLen);
	
	for (int i = -tangentLenI; i < tangentLenI; i++)
	{
		glm::vec3 translation1 = m_position + bitangent * 0.5f + tangentNorm * ((i + 0.5f) * 0.5f);
		AddBorderModelInstance(glm::mat4x3(tcb, -bitangentNorm, tangentNorm, translation1), m_opacity);
		
		glm::vec3 translation2 = m_position - bitangent * 0.5f + tangentNorm * ((i + 0.5f) * 0.5f);
		AddBorderModelInstance(glm::mat4x3(-tcb, bitangentNorm, tangentNorm, translation2), m_opacity);
	}
	
	for (int i = -bitangentLenI; i < bitangentLenI; i++)
	{
		glm::vec3 translation1 = m_position + tangent * 0.5f + bitangentNorm * ((i + 0.5f) * 0.5f);
		AddBorderModelInstance(glm::mat4x3(-tcb, -tangentNorm, bitangentNorm, translation1), 0);
		
		glm::vec3 translation2 = m_position - tangent * 0.5f + bitangentNorm * ((i + 0.5f) * 0.5f);
		AddBorderModelInstance(glm::mat4x3(tcb, tangentNorm, bitangentNorm, translation2), 0);
	}
}

int GravityBarrierEnt::BlockedAxis() const
{
	if (!m_enabled)
		return -1;
	if (m_blockFalling)
		return m_aaQuad.upPlane;
	auto [tangent, bitangent] = GetTangents();
	if (std::abs(tangent.x) > 0.5f)
		return 0;
	if (std::abs(tangent.y) > 0.5f)
		return 1;
	return 2;
}

void GravityBarrierEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	Ent::RenderSettings();
	
	m_waterBlockComponentOutOfDate |= ImGui::DragFloat2("Size", &m_aaQuad.radius.x, 0.5f);
	
	m_waterBlockComponentOutOfDate |= ImGui::Combo("Plane", &m_aaQuad.upPlane, "X\0Y\0Z\0");
	
	int flowDir = flowDirection + 1;
	if (ImGui::SliderInt("Flow Direction", &flowDir, 1, 4))
	{
		flowDirection = glm::clamp(flowDir, 1, 4) - 1;
	}
	
	ImGui::Combo("Activate action", reinterpret_cast<int*>(&activateAction), "Disable\0Enable\0Rotate\0");
	
	m_waterBlockComponentOutOfDate |= ImGui::Checkbox("Block falling", &m_blockFalling);
	
	m_waterBlockComponentOutOfDate |= ImGui::Checkbox("Never block water", &m_neverBlockWater);
	
	ImGui::Checkbox("Red from water", &m_redFromWater);
#endif
}

const void* GravityBarrierEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatableComp))
		return &m_activatable;
	if (type == typeid(AxisAlignedQuadComp))
		return &m_aaQuad;
	if (type == typeid(WaterBlockComp))
		return &m_waterBlockComp;
	return nullptr;
}

void GravityBarrierEnt::Update(const WorldUpdateArgs& args)
{
	if (args.player != nullptr)
	{
		//Updates enabled and flow direction depdending on whether the barrier is activated
		m_enabled = true;
		int newFlowDirectionOffset = 0;
		if (m_activatable.m_enabledConnections != 0)
		{
			bool activated = m_activatable.AllSourcesActive();
			switch (activateAction)
			{
			case ActivateAction::Disable:
				m_enabled = !activated;
				break;
			case ActivateAction::Enable:
				m_enabled = activated;
				break;
			case ActivateAction::Rotate:
				newFlowDirectionOffset += (int)activated;
				break;
			}
		}
		
		bool decOpacity = !m_enabled;
		
		if (newFlowDirectionOffset != m_flowDirectionOffset)
		{
			if (m_opacity < 0.01f)
			{
				m_flowDirectionOffset = newFlowDirectionOffset;
			}
			else
			{
				decOpacity = true;
			}
		}
		
		m_waterBlockComponentOutOfDate = true;
		
		constexpr float OPACITY_ANIMATION_TIME = 0.25f;
		if (decOpacity)
			m_opacity = std::max(m_opacity - args.dt / OPACITY_ANIMATION_TIME, 0.0f);
		else
			m_opacity = std::min(m_opacity + args.dt / OPACITY_ANIMATION_TIME, 1.0f);
	}
	
	if (m_waterBlockComponentOutOfDate)
	{
		m_waterBlockComponentOutOfDate = false;
		UpdateWaterBlockedGravities();
		m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_position);
		m_waterBlockComp.editorVersion++;
	}
	
	UpdateNearEntities(args.player, args.world->entManager);
}

static uint64_t lastFrameUpdatedNearEntities = UINT64_MAX;

static float* animationSpeed = eg::TweakVarFloat("gb_anim_speed", 0.05f, 0.0f);

void GravityBarrierEnt::UpdateNearEntities(const Player* player, EntityManager& entityManager)
{
	if (eg::FrameIdx() == lastFrameUpdatedNearEntities)
		return;
	lastFrameUpdatedNearEntities = eg::FrameIdx();
	
	GravityBarrierMaterial::BarrierBufferData bufferData;
	memset(&bufferData, 0, sizeof(bufferData));
	bufferData.gameTime = RenderSettings::instance->gameTime * *animationSpeed;
	int itemsWritten = 0;
	
	auto AddInteractable = [&] (Dir down, const glm::vec3& pos)
	{
		bufferData.iaDownAxis[itemsWritten] = (int)down / 2;
		for (int i = 0; i < 3; i++)
		{
			bufferData.iaPosition[itemsWritten][i] = pos[i];
		}
		itemsWritten++;
	};
	
	if (player != nullptr)
	{
		AddInteractable(player->CurrentDown(), player->Position());
	}
	
	entityManager.ForEachWithFlag(EntTypeFlags::Interactable, [&](const Ent& entity)
	{
		auto* comp = entity.GetComponent<GravityBarrierInteractableComp>();
		if (comp != nullptr && itemsWritten < GravityBarrierMaterial::NUM_INTERACTABLES)
		{
			AddInteractable(comp->currentDown, entity.GetPosition());
		}
	});
	
	while (itemsWritten < GravityBarrierMaterial::NUM_INTERACTABLES)
	{
		bufferData.iaDownAxis[itemsWritten++] = 3;
	}
	
	GravityBarrierMaterial::UpdateSharedDataBuffer(bufferData);
}

std::tuple<glm::vec3, glm::vec3> GravityBarrierEnt::GetTangents() const
{
	return m_aaQuad.GetTangents((float)(flowDirection + m_flowDirectionOffset) * eg::HALF_PI);
}

void GravityBarrierEnt::Spawned(bool isEditor)
{
	if (!isEditor)
	{
		glm::vec3 radius(GetTransform() * glm::vec4(0.5, 0.5, 0.1f, 0.0f));
		m_physicsObject.shape = eg::AABB(-radius, radius);
		m_physicsObject.position = m_position;
	}
}

void GravityBarrierEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::GravityBarrierEntity gravBarrierPB;
	
	SerializePos(gravBarrierPB, m_position);
	
	gravBarrierPB.set_flow_direction(flowDirection);
	gravBarrierPB.set_up_plane(m_aaQuad.upPlane);
	gravBarrierPB.set_sizex(m_aaQuad.radius.x);
	gravBarrierPB.set_sizey(m_aaQuad.radius.y);
	gravBarrierPB.set_activate_action((uint32_t)activateAction);
	gravBarrierPB.set_block_falling(m_blockFalling);
	gravBarrierPB.set_never_block_water(m_neverBlockWater);
	gravBarrierPB.set_red_from_water(m_redFromWater);
	
	gravBarrierPB.set_name(m_activatable.m_name);
	
	gravBarrierPB.SerializeToOstream(&stream);
}

void GravityBarrierEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::GravityBarrierEntity gravBarrierPB;
	gravBarrierPB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(gravBarrierPB);
	
	if (gravBarrierPB.name() != 0)
		m_activatable.m_name = gravBarrierPB.name();
	
	flowDirection = gravBarrierPB.flow_direction();
	m_aaQuad.upPlane = gravBarrierPB.up_plane();
	m_aaQuad.radius = glm::vec2(gravBarrierPB.sizex(), gravBarrierPB.sizey());
	activateAction = (ActivateAction)gravBarrierPB.activate_action();
	m_blockFalling = gravBarrierPB.block_falling();
	m_neverBlockWater = gravBarrierPB.never_block_water();
	m_redFromWater = gravBarrierPB.red_from_water();
	
	UpdateWaterBlockedGravities();
	m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_position);
}

void GravityBarrierEnt::UpdateWaterBlockedGravities()
{
	const int blockedAxis = BlockedAxis();
	for (int i = 0; i < 6; i++)
	{
		m_waterBlockComp.blockedGravities[i] = !m_neverBlockWater && blockedAxis == i / 2;
	}
}

void GravityBarrierEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

void GravityBarrierEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	m_waterBlockComponentOutOfDate = true;
}

int GravityBarrierEnt::EdGetIconIndex() const
{
	return -1;
}

std::span<const EditorSelectionMesh> GravityBarrierEnt::EdGetSelectionMeshes() const
{
	return m_aaQuad.GetEditorSelectionMeshes(m_position, 0.1f);
}
