#include "FloorButtonEnt.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Protobuf/Build/FloorButtonEntity.pb.h"

static eg::Model* s_model;
static eg::IMaterial* s_material;

static void OnInit()
{
	s_model = &eg::GetAsset<eg::Model>("Models/Button.obj");
	s_material = &eg::GetAsset<StaticPropMaterial>("Materials/Button.yaml");
}

EG_ON_INIT(OnInit)

void FloorButtonEnt::Draw(const EntDrawArgs& args)
{
	args.meshBatch->AddModel(*s_model, *s_material, GetTransform(SCALE));
}

void FloorButtonEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	args.meshBatch->AddModel(*s_model, *s_material, GetTransform(SCALE));
}

const void* FloorButtonEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatorComp))
		return &m_activator;
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

void FloorButtonEnt::Update(const struct WorldUpdateArgs& args)
{
	m_activator.Update(args);
}
