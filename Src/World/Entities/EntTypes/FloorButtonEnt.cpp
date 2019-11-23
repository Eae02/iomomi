#include "FloorButtonEnt.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Protobuf/Build/FloorButtonEntity.pb.h"
#include "../../WorldUpdateArgs.hpp"

static eg::Model* s_model;
static eg::IMaterial* s_material;
static size_t s_padMeshIndex;

static eg::CollisionMesh buttonCollisionMeshEG;
static std::unique_ptr<btTriangleMesh> buttonCollisionMesh;
static std::unique_ptr<btCollisionShape> buttonCollisionShape;

static void OnInit()
{
	s_model = &eg::GetAsset<eg::Model>("Models/Button.obj");
	s_material = &eg::GetAsset<StaticPropMaterial>("Materials/Button.yaml");
	
	for (size_t i = 0; i < s_model->NumMeshes(); i++)
	{
		if (s_model->GetMesh(i).name.find("Pad") != std::string::npos)
		{
			s_padMeshIndex = i;
		}
		else if (s_model->GetMesh(i).name.find("Circle") != std::string::npos)
		{
			buttonCollisionMesh = std::make_unique<btTriangleMesh>();
			
			auto meshData = s_model->GetMeshData<eg::StdVertex, uint32_t>(i);
			buttonCollisionMeshEG = eg::CollisionMesh::Create(meshData.vertices, meshData.indices);
			bullet::AddCollisionMesh(*buttonCollisionMesh, buttonCollisionMeshEG);
			
			buttonCollisionShape = std::make_unique<btBvhTriangleMeshShape>(buttonCollisionMesh.get(), true);
		}
	}
}

EG_ON_INIT(OnInit)

FloorButtonEnt::FloorButtonEnt()
{
	m_rigidBody.InitStatic(*buttonCollisionShape);
}

static float* floorButtonPushDist = eg::TweakVarFloat("fbtn_push_dist", 0.06f, 0.0f);

void FloorButtonEnt::Draw(const EntDrawArgs& args)
{
	for (size_t i = 0; i < s_model->NumMeshes(); i++)
	{
		glm::mat4 transform = GetTransform(SCALE);
		if (i == s_padMeshIndex)
		{
			transform = transform * glm::translate(glm::mat4(1), glm::vec3(0, -m_padPushDist * *floorButtonPushDist, 0));
		}
		args.meshBatch->AddModelMesh(*s_model, i, *s_material, transform);
	}
}

void FloorButtonEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	args.meshBatch->AddModel(*s_model, *s_material, GetTransform(SCALE));
}

void FloorButtonEnt::Update(const struct WorldUpdateArgs& args)
{
	m_activator.Update(args);
	
	constexpr float PAD_PUSH_TIME = 0.2f;
	if (m_activator.IsActivated())
	{
		m_padPushDist = std::min(m_padPushDist + args.dt / PAD_PUSH_TIME, 1.0f);
	}
	else
	{
		m_padPushDist = std::max(m_padPushDist - args.dt / PAD_PUSH_TIME, 0.0f);
	}
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
	return Ent::GetAABB(SCALE * *floorButtonAABBScale, 0.2f);
}
