#include "FloorButtonEnt.hpp"
#include "../../WorldUpdateArgs.hpp"
#include "../../World.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../../Protobuf/Build/FloorButtonEntity.pb.h"
#include <iomanip>

static eg::Model* s_model;
static eg::IMaterial* s_material;
static size_t s_padMeshIndex;
static size_t s_lightsMeshIndex;

static eg::CollisionMesh frameCollisionMeshEG;
static btTriangleMesh frameCollisionMesh;
static std::unique_ptr<btCollisionShape> frameCollisionShape;

static eg::CollisionMesh buttonCollisionMeshEG;
static btTriangleMesh buttonCollisionMesh;
static std::unique_ptr<btCollisionShape> buttonCollisionShape;

static void OnInit()
{
	s_model = &eg::GetAsset<eg::Model>("Models/Button.obj");
	s_material = &eg::GetAsset<StaticPropMaterial>("Materials/Button.yaml");
	
	for (size_t i = 0; i < s_model->NumMeshes(); i++)
	{
		if (s_model->GetMesh(i).name.find("Pad") != std::string::npos)
		{
			auto meshData = s_model->GetMeshData<eg::StdVertex, uint32_t>(i);
			buttonCollisionMeshEG = eg::CollisionMesh::Create(meshData.vertices, meshData.indices);
			bullet::AddCollisionMesh(buttonCollisionMesh, buttonCollisionMeshEG);
			buttonCollisionShape = std::make_unique<btBvhTriangleMeshShape>(&buttonCollisionMesh, true);
			
			s_padMeshIndex = i;
		}
		else if (s_model->GetMesh(i).name.find("Circle") != std::string::npos)
		{
			auto meshData = s_model->GetMeshData<eg::StdVertex, uint32_t>(i);
			frameCollisionMeshEG = eg::CollisionMesh::Create(meshData.vertices, meshData.indices);
			bullet::AddCollisionMesh(frameCollisionMesh, frameCollisionMeshEG);
			frameCollisionShape = std::make_unique<btBvhTriangleMeshShape>(&frameCollisionMesh, true);
		}
		else if (s_model->GetMesh(i).name.find("Lights") != std::string::npos)
		{
			s_lightsMeshIndex = i;
		}
	}
}

EG_ON_INIT(OnInit)

constexpr float MAX_PUSH_DST = 0.07f;
constexpr float ACTIVATE_PUSH_DST = MAX_PUSH_DST * 0.8f;

FloorButtonEnt::FloorButtonEnt()
{
	m_frameRigidBody = &m_rigidBody.InitStatic(this, *frameCollisionShape);
	m_buttonRigidBody = &m_rigidBody.Init(this, 0.01f, *buttonCollisionShape);
	m_buttonRigidBody->setIgnoreCollisionCheck(m_frameRigidBody, true);
	m_buttonRigidBody->setFlags(m_buttonRigidBody->getFlags() | BT_DISABLE_WORLD_GRAVITY);
}

void FloorButtonEnt::CommonDraw(const EntDrawArgs& args)
{
	for (size_t i = 0; i < s_model->NumMeshes(); i++)
	{
		glm::mat4 transform = GetTransform(1);
		if (i == s_padMeshIndex)
		{
			transform = transform * glm::translate(glm::mat4(1), glm::vec3(0, -m_padPushDist, 0));
		}
		
		if (i == s_lightsMeshIndex)
		{
			float a = glm::clamp(m_padPushDist / ACTIVATE_PUSH_DST, 0.0f, 1.0f);
			
			glm::vec3 colorD = {
				ActivationLightStripEnt::DEACTIVATED_COLOR.r,
				ActivationLightStripEnt::DEACTIVATED_COLOR.g,
				ActivationLightStripEnt::DEACTIVATED_COLOR.b
			};
			glm::vec3 colorA = {
				ActivationLightStripEnt::ACTIVATED_COLOR.r,
				ActivationLightStripEnt::ACTIVATED_COLOR.g,
				ActivationLightStripEnt::ACTIVATED_COLOR.b
			};
			
			EmissiveMaterial::InstanceData instanceData;
			instanceData.transform = transform;
			instanceData.color = glm::vec4(glm::mix(colorD, colorA, a), 1.0f);
			args.meshBatch->AddModelMesh(*s_model, i, EmissiveMaterial::instance, instanceData);
		}
		else
		{
			args.meshBatch->AddModelMesh(*s_model, i, *s_material, StaticPropMaterial::InstanceData(transform));
		}
	}
}

void FloorButtonEnt::Update(const WorldUpdateArgs& args)
{
	constexpr int PUSH_AXIS = 1;
	
	if (args.world->PhysicsWorld() && !m_springConstraint)
	{
		btTransform localTransform;
		localTransform.setIdentity();
		
		m_springConstraint = std::make_unique<btGeneric6DofSpring2Constraint>(
			*m_frameRigidBody, *m_buttonRigidBody, localTransform, localTransform);
		m_springConstraint->setLimit(PUSH_AXIS, -MAX_PUSH_DST, 0.0f);
		m_springConstraint->enableSpring(PUSH_AXIS, true);
		m_springConstraint->setStiffness(PUSH_AXIS, 50);
		m_springConstraint->setDamping(PUSH_AXIS, 0);
		m_springConstraint->setEquilibriumPoint(PUSH_AXIS);
		args.world->PhysicsWorld()->addConstraint(m_springConstraint.get());
	}
	
	if (m_springConstraint)
	{
		m_springConstraint->calculateTransforms();
		m_padPushDist = glm::clamp(glm::distance(bullet::ToGLM(m_springConstraint->getCalculatedTransformB().getOrigin()), m_position), 0.0f, MAX_PUSH_DST);
		
		if (m_padPushDist > ACTIVATE_PUSH_DST)
			m_activator.Activate();
	}
	
	m_activator.Update(args);
}

const void* FloorButtonEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatorComp))
		return &m_activator;
	if (type == typeid(RigidBodyComp))
		return &m_rigidBody;
	return Ent::GetComponent(type);
}

void FloorButtonEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::FloorButtonEntity buttonPB;
	
	buttonPB.set_dir((gravity_pb::Dir)m_direction);
	SerializePos(buttonPB);
	
	buttonPB.set_allocated_activator(m_activator.SaveProtobuf(nullptr));
	
	buttonPB.SerializeToOstream(&stream);
}

void FloorButtonEnt::Deserialize(std::istream& stream)
{
	gravity_pb::FloorButtonEntity buttonPB;
	buttonPB.ParseFromIstream(&stream);
	
	m_direction = (Dir)buttonPB.dir();
	DeserializePos(buttonPB);
	
	m_rigidBody.SetTransform(m_position, glm::quat_cast(GetRotationMatrix()));
	
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

static float* floorButtonAABBScale = eg::TweakVarFloat("fbtn_aabb_scale", 1.0f, 0.0f);

eg::AABB FloorButtonEnt::GetAABB() const
{
	return Ent::GetAABB(*floorButtonAABBScale, 0.2f);
}
