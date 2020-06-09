#include "GravitySwitchEnt.hpp"
#include "../../Player.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Materials/GravitySwitchMaterial.hpp"
#include "../../../../Protobuf/Build/GravitySwitchEntity.pb.h"
#include "../../../Graphics/RenderSettings.hpp"

static eg::Model* s_model;
static eg::IMaterial* s_material;
static int s_centerMaterialIndex;

static void OnInit()
{
	s_model = &eg::GetAsset<eg::Model>("Models/GravitySwitch.obj");
	s_material = &eg::GetAsset<StaticPropMaterial>("Materials/GravitySwitch.yaml");
	s_centerMaterialIndex = s_model->GetMaterialIndex("Center");
}

EG_ON_INIT(OnInit)

GravitySwitchEnt::GravitySwitchEnt()
	: m_activatable(&GravitySwitchEnt::GetConnectionPoints) { }

std::vector<glm::vec3> GravitySwitchEnt::GetConnectionPoints(const Ent& entity)
{
	const GravitySwitchEnt& switchEnt = (const GravitySwitchEnt&)entity;
	
	const glm::mat4 transform =
		glm::translate(glm::mat4(1), switchEnt.m_position) *
		glm::mat4(GetRotationMatrix(switchEnt.m_up));
	
	const glm::vec3 connectionPoints[] = 
	{
		{ 1, 0, 0 },
		{ -1, 0, 0 },
		{ 0, 0, 1 },
		{ 0, 0, -1 },
	};
	
	std::vector<glm::vec3> connectionPointsWS(std::size(connectionPoints));
	for (size_t i = 0; i < std::size(connectionPoints); i++)
	{
		connectionPointsWS[i] = glm::vec3(transform * glm::vec4(connectionPoints[i] * 0.7f, 1));
	}
	
	return connectionPointsWS;
}

constexpr float ENABLE_ANIMATION_TIME = 0.2f;

void GravitySwitchEnt::Update(const WorldUpdateArgs& args)
{
	m_enableAnimationTime = eg::AnimateTo(m_enableAnimationTime,
		m_activatable.AllSourcesActive() ? 1.0f : 0.0f, args.dt / ENABLE_ANIMATION_TIME);
	
	m_centerMaterial.timeOffset += glm::mix(0.2f, 1.0f, m_enableAnimationTime) * args.dt;
}

void GravitySwitchEnt::Draw(eg::MeshBatch& meshBatch) const
{
	const glm::mat4 transform = glm::translate(glm::mat4(1), m_position) * glm::mat4(GetRotationMatrix(m_up));
	
	for (size_t m = 0; m < s_model->NumMeshes(); m++)
	{
		const eg::IMaterial* material = s_material;
		if (s_model->GetMesh(m).materialIndex == s_centerMaterialIndex)
		{
			material = &m_centerMaterial;
		}
		meshBatch.AddModelMesh(*s_model, m, *material, StaticPropMaterial::InstanceData(transform));
	}
}

void GravitySwitchEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	m_centerMaterial.timeOffset = 0;
	m_centerMaterial.intensity = m_activatable.AllSourcesActive() ? 1 : 0.25f;
	Draw(*args.meshBatch);
}

void GravitySwitchEnt::GameDraw(const EntGameDrawArgs& args)
{
	m_centerMaterial.intensity = glm::mix(0.25f, 1.0f, m_enableAnimationTime);
	Draw(*args.meshBatch);
	
	if (m_enableAnimationTime > 0.01f)
	{
		m_volLightMaterial.intensity = m_enableAnimationTime;
		m_volLightMaterial.rotationMatrix = GetRotationMatrix(m_up);
		m_volLightMaterial.switchPosition = m_position;
		args.transparentMeshBatch->AddNoData(GravitySwitchVolLightMaterial::GetMesh(), m_volLightMaterial,
		                                     DepthDrawOrder(m_volLightMaterial.switchPosition));
	}
}

void GravitySwitchEnt::Interact(Player& player)
{
	player.FlipDown();
}

int GravitySwitchEnt::CheckInteraction(const Player& player, const class PhysicsEngine& physicsEngine) const
{
	bool canInteract =
		m_activatable.AllSourcesActive() &&
		player.CurrentDown() == OppositeDir(m_up) && player.OnGround() &&
		GetAABB().Contains(player.FeetPosition()) && !player.m_isCarrying;
	
	constexpr int INTERACT_PRIORITY = 1;
	return canInteract ? INTERACT_PRIORITY : 0;
}

std::string_view GravitySwitchEnt::GetInteractDescription() const
{
	return "Flip Gravity";
}

void GravitySwitchEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::GravitySwitchEntity switchPB;
	
	switchPB.set_dir((gravity_pb::Dir)m_up);
	SerializePos(switchPB, m_position);
	
	switchPB.set_name(m_activatable.m_name);
	
	switchPB.SerializeToOstream(&stream);
}

void GravitySwitchEnt::Deserialize(std::istream& stream)
{
	gravity_pb::GravitySwitchEntity switchPB;
	switchPB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(switchPB);
	m_up = (Dir)switchPB.dir();
	
	if (switchPB.name() != 0)
		m_activatable.m_name = switchPB.name();
}

const void* GravitySwitchEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatableComp))
		return &m_activatable;
	return nullptr;
}

void GravitySwitchEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	if (faceDirection)
		m_up = *faceDirection;
}
