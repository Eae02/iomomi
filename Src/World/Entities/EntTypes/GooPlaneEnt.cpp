#include "GooPlaneEnt.hpp"
#include "../Entity.hpp"
#include "../../../../Protobuf/Build/GooPlaneEntity.pb.h"

GooPlaneEnt::GooPlaneEnt()
{
	m_liquidPlane.shouldGenerateMesh = true;
	m_liquidPlane.editorColor = eg::ColorSRGB::FromRGBAHex(0x619F4996);
}

void GooPlaneEnt::RenderSettings()
{
	Ent::RenderSettings();
}

void GooPlaneEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_liquidPlane.position = newPosition;
	if (faceDirection)
		m_liquidPlane.wallForward = *faceDirection;
	m_liquidPlane.MarkOutOfDate();
}

void GooPlaneEnt::GameDraw(const EntGameDrawArgs& args)
{
	m_liquidPlane.MaybeUpdate(*args.world);
	
	if (m_liquidPlane.NumIndices() != 0)
	{
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
	
	SerializePos(gooPlanePB, m_liquidPlane.position);
	gooPlanePB.set_wall_dir((gravity_pb::Dir)m_liquidPlane.wallForward);
	
	gooPlanePB.SerializeToOstream(&stream);
}

void GooPlaneEnt::Deserialize(std::istream& stream)
{
	gravity_pb::GooPlaneEntity gooPlanePB;
	gooPlanePB.ParseFromIstream(&stream);
	m_liquidPlane.position = DeserializePos(gooPlanePB);
	m_liquidPlane.wallForward = (Dir)gooPlanePB.wall_dir();
}
