#include "RampEnt.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Protobuf/Build/Ramp.pb.h"

#include <imgui.h>

static const glm::vec3 untransformedPositions[] = 
{
	{ -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, 1 },
	{ 1, -1, -1 }, { 1, 1, 1 }, { -1, 1, 1 }
};
static const glm::vec2 uvs[] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

RampEnt::RampEnt()
{
	
}

void RampEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	if (ImGui::DragFloat3("Size", &m_size.x))
		m_vertexBufferOutOfDate = true;
	if (ImGui::SliderInt("Yaw", &m_yaw, 0, 3))
		m_vertexBufferOutOfDate = true;
	if (ImGui::Checkbox("Flipped", &m_flipped))
		m_vertexBufferOutOfDate = true;
}

void RampEnt::Spawned(bool isEditor)
{
	if (isEditor)
	{
		m_position += glm::vec3(DirectionVector(m_direction)) * 0.5f;
	}
}

void RampEnt::CommonDraw(const EntDrawArgs& args)
{
	InitializeVertexBuffer();
	
	eg::MeshBatch::Mesh mesh;
	mesh.firstIndex = 0;
	mesh.firstVertex = 0;
	mesh.numElements = 6;
	mesh.vertexBuffer = m_vertexBuffer;
	args.meshBatch->Add(mesh, eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"),
		StaticPropMaterial::InstanceData(glm::translate(glm::mat4(1), Pos())));
}

void RampEnt::InitializeVertexBuffer()
{
	if (!m_vertexBufferOutOfDate && m_vertexBuffer.handle)
		return;
	m_vertexBufferOutOfDate = false;
	
	if (!m_vertexBuffer.handle)
	{
		m_vertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer | eg::BufferFlags::Update,
			sizeof(eg::StdVertex) * 6, nullptr);
	}
	
	std::array<glm::vec3, 6> transformedPos = GetTransformedVertices();
	
	glm::vec3 tangent = glm::normalize(transformedPos[1] - transformedPos[0]);
	glm::vec3 normal = glm::normalize(glm::cross(tangent, transformedPos[2] - transformedPos[0]));
	
	float uvScaleV = glm::length(glm::vec2(m_size.y, m_size.z));
	
	eg::StdVertex vertices[6] = { };
	for (int i = 0; i < 6; i++)
	{
		*reinterpret_cast<glm::vec3*>(vertices[i].position) = transformedPos[i];
		for (int j = 0; j < 3; j++)
		{
			vertices[i].normal[j] = eg::FloatToSNorm(normal[j]);
			vertices[i].tangent[j] = eg::FloatToSNorm(tangent[j]);
		}
		vertices[i].texCoord[0] = uvs[i].x * m_size.x;
		vertices[i].texCoord[1] = uvs[i].y * uvScaleV;
	}
	
	eg::DC.UpdateBuffer(m_vertexBuffer, 0, sizeof(vertices), vertices);
	m_vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
}

std::array<glm::vec3, 6> RampEnt::GetTransformedVertices() const
{
	glm::vec3 size = 0.5f * m_size;
	if (m_flipped)
		size.y = -size.y;
	
	glm::mat4 transformMatrix =
		glm::rotate(glm::mat4(1), (float)m_yaw * eg::HALF_PI, glm::vec3(0, 1, 0)) *
		glm::scale(glm::mat4(1), size);
	
	std::array<glm::vec3, 6> transformedPos;
	for (int i = 0; i < 6; i++)
	{
		transformedPos[i] = transformMatrix * glm::vec4(untransformedPositions[i], 1.0f);
	}
	return transformedPos;
}

void RampEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::RampEntity rampPB;
	
	SerializePos(rampPB);
	
	rampPB.set_yaw(m_yaw);
	rampPB.set_flipped(m_flipped);
	rampPB.set_scalex(m_size.x);
	rampPB.set_scaley(m_size.y);
	rampPB.set_scalez(m_size.z);
	
	rampPB.SerializeToOstream(&stream);
}

void RampEnt::Deserialize(std::istream& stream)
{
	gravity_pb::RampEntity rampPB;
	rampPB.ParseFromIstream(&stream);
	
	m_yaw = rampPB.yaw();
	m_flipped = rampPB.flipped();
	m_size.x = rampPB.scalex();
	m_size.y = rampPB.scaley();
	m_size.z = rampPB.scalez();
	DeserializePos(rampPB);
	
	m_vertexBufferOutOfDate = true;
	std::array<glm::vec3, 6> vertices = GetTransformedVertices();
	
	m_collisionMesh.addTriangle(bullet::FromGLM(vertices[0]), bullet::FromGLM(vertices[1]), bullet::FromGLM(vertices[2]));
	m_collisionMesh.addTriangle(bullet::FromGLM(vertices[3]), bullet::FromGLM(vertices[4]), bullet::FromGLM(vertices[5]));
	m_collisionShape = std::make_unique<btBvhTriangleMeshShape>(&m_collisionMesh, true);
	
	m_rigidBody.InitStatic(this, *m_collisionShape);
	m_rigidBody.SetTransform(m_position, glm::quat());
	m_rigidBody.GetRigidBody()->setFriction(1.0f);
}

const void* RampEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(RigidBodyComp))
		return &m_rigidBody;
	return Ent::GetComponent(type);
}

template <>
std::shared_ptr<Ent> CloneEntity<RampEnt>(const Ent& entity)
{
	const RampEnt& src = static_cast<const RampEnt&>(entity);
	std::shared_ptr<RampEnt> clone = Ent::Create<RampEnt>();
	clone->m_yaw = src.m_yaw;
	clone->m_flipped = src.m_flipped;
	clone->m_size = src.m_size;
	return clone;
}
