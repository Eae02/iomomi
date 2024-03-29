#include "FloorButtonEnt.hpp"

#include "../../../../../Protobuf/Build/FloorButtonEntity.pb.h"
#include "../../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../ImGui.hpp"
#include "../../../World.hpp"
#include "../../../WorldUpdateArgs.hpp"

DEF_ENT_TYPE(FloorButtonEnt)

static eg::Model* s_model;
static eg::CollisionMesh s_collisionMesh;
static eg::IMaterial* s_material;
static size_t s_padMeshIndex;
static size_t s_lightsMeshIndex;

static eg::CollisionMesh s_ringCollisionMesh;
static eg::CollisionMesh s_padCollisionMesh;

static void OnInit()
{
	eg::Model& collisionModel = eg::GetAsset<eg::Model>("Models/Button.col.obj");
	s_ringCollisionMesh = collisionModel.MakeCollisionMesh(collisionModel.GetMeshIndex("CollisionRing"));
	s_padCollisionMesh = collisionModel.MakeCollisionMesh(collisionModel.GetMeshIndex("CollisionPad"));

	s_model = &eg::GetAsset<eg::Model>("Models/Button.aa.obj");
	s_collisionMesh = s_model->MakeCollisionMesh();
	s_material = &eg::GetAsset<StaticPropMaterial>("Materials/Button.yaml");

	for (size_t i = 0; i < s_model->NumMeshes(); i++)
	{
		if (s_model->GetMesh(i).name.find("Pad") != std::string::npos)
		{
			s_padMeshIndex = i;
		}
		else if (s_model->GetMesh(i).name.find("Lights") != std::string::npos)
		{
			s_lightsMeshIndex = i;
		}
	}
}

EG_ON_INIT(OnInit)

constexpr float LIGHT_ANIMATION_TIME = 0.1f;
constexpr float MAX_PUSH_DST = 0.09f;
constexpr float ACTIVATE_PUSH_DST = MAX_PUSH_DST * 0.8f;

FloorButtonEnt::FloorButtonEnt()
{
	m_ringPhysicsObject.canBePushed = false;
	m_ringPhysicsObject.owner = this;
	m_ringPhysicsObject.debugColor = 0x12b81a;
	m_ringPhysicsObject.shouldCollide = &FloorButtonEnt::ShouldCollide;
	m_ringPhysicsObject.shape = &s_ringCollisionMesh;

	m_padPhysicsObject.canBePushed = true;
	m_padPhysicsObject.owner = this;
	m_padPhysicsObject.debugColor = 0xcf24cf;
	m_padPhysicsObject.shape = &s_padCollisionMesh;
	m_padPhysicsObject.shouldCollide = &FloorButtonEnt::ShouldCollide;
	m_padPhysicsObject.constrainMove = &FloorButtonEnt::ConstrainMove;
}

void FloorButtonEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	ImGui::SliderFloat("Linger Time", &m_lingerTime, 0.0f, 2.0f, "%.2f s");
#endif

	Ent::RenderSettings();
}

bool FloorButtonEnt::ShouldCollide(const PhysicsObject& self, const PhysicsObject& other)
{
	// Disables collision between the button ring and cubes
	if (!self.canBePushed && std::holds_alternative<Ent*>(other.owner) &&
	    std::get<Ent*>(other.owner)->TypeID() == EntTypeID::Cube)
	{
		return false;
	}

	return self.owner != other.owner;
}

glm::vec3 FloorButtonEnt::ConstrainMove(const PhysicsObject& object, const glm::vec3& move)
{
	FloorButtonEnt& ent = *(FloorButtonEnt*)std::get<Ent*>(object.owner);
	glm::vec3 moveDir = ent.GetRotationMatrix(ent.m_direction) * glm::vec3(0, -1, 0);

	float posSlideTime = glm::dot(object.position - ent.GetPosition(), moveDir) / glm::length2(moveDir);
	float slideDist = glm::dot(move, moveDir) / glm::length2(moveDir);

	return moveDir * glm::clamp(slideDist, -posSlideTime, MAX_PUSH_DST - posSlideTime);
}

void FloorButtonEnt::CommonDraw(const EntDrawArgs& args)
{
	for (size_t i = 0; i < s_model->NumMeshes(); i++)
	{
		if ((i == s_padMeshIndex || i == s_lightsMeshIndex) && args.shadowDrawArgs)
			continue;

		glm::vec3 displayPosition = (i == s_padMeshIndex ? m_padPhysicsObject : m_ringPhysicsObject).displayPosition;
		glm::mat4 transform = glm::translate(glm::mat4(1), displayPosition) * glm::mat4(GetRotationMatrix(m_direction));

		const eg::AABB aabb = s_model->GetMesh(i).boundingAABB->TransformedBoundingBox(transform);
		if (!args.frustum->Intersects(aabb))
			continue;

		if (i == s_lightsMeshIndex)
		{
			glm::vec3 colorD = { ActivationLightStripEnt::DEACTIVATED_COLOR.r,
				                 ActivationLightStripEnt::DEACTIVATED_COLOR.g,
				                 ActivationLightStripEnt::DEACTIVATED_COLOR.b };
			glm::vec3 colorA = { ActivationLightStripEnt::ACTIVATED_COLOR.r, ActivationLightStripEnt::ACTIVATED_COLOR.g,
				                 ActivationLightStripEnt::ACTIVATED_COLOR.b };

			EmissiveMaterial::InstanceData instanceData;
			instanceData.transform = transform;
			instanceData.color = glm::vec4(glm::mix(colorD, colorA, glm::smoothstep(0.0f, 1.0f, m_lightColor)), 1.0f);
			args.meshBatch->AddModelMesh(*s_model, i, EmissiveMaterial::instance, instanceData);
		}
		else
		{
			args.meshBatch->AddModelMesh(*s_model, i, *s_material, StaticPropMaterial::InstanceData(transform));
		}
	}
}

static float* floorButtonAABBScale = eg::TweakVarFloat("fbtn_aabb_scale", 1.0f, 0.0f);

void FloorButtonEnt::Update(const WorldUpdateArgs& args)
{
	float pushDist = glm::distance(m_padPhysicsObject.position, GetPosition());
	if (pushDist > ACTIVATE_PUSH_DST)
	{
		m_timeSinceActivated = 0;
	}
	else
	{
		m_timeSinceActivated += args.dt;
	}

	if (m_timeSinceActivated <= m_lingerTime)
	{
		m_activator.Activate();
		m_lightColor = std::min(m_lightColor + args.dt / LIGHT_ANIMATION_TIME, 1.0f);
	}
	else
	{
		m_lightColor = std::max(m_lightColor - args.dt / LIGHT_ANIMATION_TIME, 0.0f);
	}

	m_padPhysicsObject.move += GetRotationMatrix(m_direction) * glm::vec3(0, args.dt * 0.2f, 0);

	m_activator.Update(args);
}

const void* FloorButtonEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatorComp))
		return &m_activator;
	return Ent::GetComponent(type);
}

void FloorButtonEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::FloorButtonEntity buttonPB;

	buttonPB.set_dir(static_cast<iomomi_pb::Dir>(m_direction));
	SerializePos(buttonPB, m_ringPhysicsObject.position);

	buttonPB.set_allocated_activator(m_activator.SaveProtobuf(nullptr));
	buttonPB.set_linger_time(m_lingerTime);

	buttonPB.SerializeToOstream(&stream);
}

void FloorButtonEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::FloorButtonEntity buttonPB;
	buttonPB.ParseFromIstream(&stream);

	m_direction = static_cast<Dir>(buttonPB.dir());
	m_ringPhysicsObject.displayPosition = m_ringPhysicsObject.position = m_padPhysicsObject.displayPosition =
		m_padPhysicsObject.position = DeserializePos(buttonPB);

	m_ringPhysicsObject.rotation = m_padPhysicsObject.rotation = glm::quat_cast(GetRotationMatrix(m_direction));
	m_lingerTime = buttonPB.linger_time();

	if (buttonPB.has_activator())
	{
		m_activator.LoadProtobuf(buttonPB.activator());
	}
	else
	{
		m_activator.targetConnectionIndex = buttonPB.source_index();
		m_activator.activatableName = buttonPB.target_name();
	}
}

eg::AABB FloorButtonEnt::GetAABB() const
{
	return Ent::GetAABB(*floorButtonAABBScale, 0.2f, m_direction);
}

void FloorButtonEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_ringPhysicsObject);
	physicsEngine.RegisterObject(&m_padPhysicsObject);
}

void FloorButtonEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_padPhysicsObject.displayPosition = m_padPhysicsObject.position = newPosition;
	m_ringPhysicsObject.displayPosition = m_ringPhysicsObject.position = newPosition;
	if (faceDirection)
	{
		m_direction = *faceDirection;
	}
}

int FloorButtonEnt::EdGetIconIndex() const
{
	return 15;
}

std::span<const EditorSelectionMesh> FloorButtonEnt::EdGetSelectionMeshes() const
{
	static EditorSelectionMesh selectionMesh;
	selectionMesh.model = s_model;
	selectionMesh.collisionMesh = &s_collisionMesh;
	selectionMesh.transform =
		glm::translate(glm::mat4(1), m_ringPhysicsObject.displayPosition) * glm::mat4(GetRotationMatrix(m_direction));
	;
	return { &selectionMesh, 1 };
}
