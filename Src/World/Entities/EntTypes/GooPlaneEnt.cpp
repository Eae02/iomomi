#include "GooPlaneEnt.hpp"
#include "../Entity.hpp"
#include "../../../../Protobuf/Build/GooPlaneEntity.pb.h"

GooPlaneEnt::GooPlaneEnt()
{
	m_liquidPlane.SetShouldGenerateMesh(true);
	m_liquidPlane.SetEditorColor(eg::ColorSRGB::FromRGBAHex(0x619F4996));
}

void GooPlaneEnt::RenderSettings()
{
	Ent::RenderSettings();
}

void GooPlaneEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	Ent::EditorMoved(newPosition, faceDirection);
}

void GooPlaneEnt::Draw(const EntDrawArgs& args)
{
	m_liquidPlane.MaybeUpdate(*this, *args.world);
	m_material.m_reflectionPlane.plane = eg::Plane(glm::vec3(0, 1, 0), m_position);
	m_material.m_reflectionPlane.texture = eg::TextureRef();
	
	if (m_liquidPlane.NumIndices() != 0)
	{
		args.reflectionPlanes->push_back(&m_material.m_reflectionPlane);
		args.meshBatch->AddNoData(m_liquidPlane.GetMesh(), m_material);
	}
}

const void* GooPlaneEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(LiquidPlaneComp))
		return &m_liquidPlane;
	return Ent::GetComponent(type);
}

void GooPlaneEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::GooPlaneEntity gooPlanePB;
	
	SerializePos(gooPlanePB);
	
	gooPlanePB.SerializeToOstream(&stream);
}

void GooPlaneEnt::Deserialize(std::istream& stream)
{
	gravity_pb::GooPlaneEntity gooPlanePB;
	gooPlanePB.ParseFromIstream(&stream);
	DeserializePos(gooPlanePB);
}

template <>
std::shared_ptr<Ent> CloneEntity<GooPlaneEnt>(const Ent& entity)
{
	std::shared_ptr<GooPlaneEnt> newEntity = Ent::Create<GooPlaneEnt>();
	newEntity->m_position = entity.Pos();
	newEntity->m_direction = entity.Direction();
	return newEntity;
}
