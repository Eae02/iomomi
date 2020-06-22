#include "RampEnt.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Protobuf/Build/Ramp.pb.h"
#include "../../Collision.hpp"

#include <imgui.h>
#include <glm/glm.hpp>

static const glm::vec3 untransformedPositions[4] = 
{
	{ -1, -1, -1 }, { 1, -1, -1 }, { -1, 1, 1 }, { 1, 1, 1 }
};
static const int indices[6] = { 0, 1, 2, 1, 3, 2 };
static const glm::vec2 uvs[4] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };

RampEnt::RampEnt()
{
	m_physicsObject.canBePushed = false;
	m_physicsObject.owner = this;
}

void RampEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	if (ImGui::DragFloat3("Size", &m_size.x))
		m_vertexBufferOutOfDate = true;
	if (ImGui::SliderInt("Rotation", &m_rotation, 0, 5))
		m_vertexBufferOutOfDate = true;
	if (ImGui::Button("Flip Normal"))
	{
		m_flipped = !m_flipped;
		m_vertexBufferOutOfDate = true;
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
		StaticPropMaterial::InstanceData(glm::translate(glm::mat4(1), m_position)));
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
	
	std::array<glm::vec3, 4> transformedPos = GetTransformedVertices();
	
	glm::vec3 tangent = glm::normalize(transformedPos[1] - transformedPos[0]);
	glm::vec3 normal = glm::normalize(glm::cross(tangent, transformedPos[2] - transformedPos[0]));
	
	float uvScaleV = glm::length(glm::vec2(m_size.y, m_size.z));
	
	eg::StdVertex vertices[6] = { };
	for (int i = 0; i < 6; i++)
	{
		*reinterpret_cast<glm::vec3*>(vertices[i].position) = transformedPos[indices[i]];
		for (int j = 0; j < 3; j++)
		{
			vertices[i].normal[j] = eg::FloatToSNorm(normal[j]);
			vertices[i].tangent[j] = eg::FloatToSNorm(tangent[j]);
		}
		vertices[i].texCoord[0] = uvs[indices[i]].x * m_size.x;
		vertices[i].texCoord[1] = uvs[indices[i]].y * uvScaleV;
	}
	
	eg::DC.UpdateBuffer(m_vertexBuffer, 0, sizeof(vertices), vertices);
	m_vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
}

std::array<glm::vec3, 4> RampEnt::GetTransformedVertices() const
{
	glm::vec3 size = 0.5f * m_size;
	
	glm::mat4 rotationMatrix;
	if (m_rotation < 4)
	{
		rotationMatrix = glm::rotate(glm::mat4(1), (float)m_rotation * eg::HALF_PI, glm::vec3(0, 1, 0));
	}
	else if (m_rotation == 4)
	{
		rotationMatrix = glm::rotate(glm::mat4(1), -eg::HALF_PI, glm::vec3(0, 0, 1));
	}
	else if (m_rotation == 5)
	{
		rotationMatrix = glm::rotate(glm::mat4(1), eg::HALF_PI, glm::vec3(0, 0, 1));
	}
	
	glm::mat4 transformMatrix = rotationMatrix * glm::scale(glm::mat4(1), size);
	
	std::array<glm::vec3, 4> transformedPos;
	for (int i = 0; i < 4; i++)
	{
		transformedPos[i] = transformMatrix * glm::vec4(untransformedPositions[i], 1.0f);
	}
	if (m_flipped)
		std::swap(transformedPos[0], transformedPos[3]);
	
	return transformedPos;
}

void RampEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::RampEntity rampPB;
	
	SerializePos(rampPB, m_position);
	
	rampPB.set_yaw(m_rotation);
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
	
	m_rotation = rampPB.yaw();
	m_flipped = rampPB.flipped();
	m_size.x = rampPB.scalex();
	m_size.y = rampPB.scaley();
	m_size.z = rampPB.scalez();
	m_position = DeserializePos(rampPB);
	
	m_vertexBufferOutOfDate = true;
	std::array<glm::vec3, 4> vertices = GetTransformedVertices();
	
	m_collisionMesh = eg::CollisionMesh::CreateV3(vertices, eg::Span<const int>(indices));
	m_collisionMesh.FlipWinding();
	m_physicsObject.position = m_position;
	m_physicsObject.shape = &m_collisionMesh;
}

void RampEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

void RampEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
}

template <>
std::shared_ptr<Ent> CloneEntity<RampEnt>(const Ent& entity)
{
	const RampEnt& src = static_cast<const RampEnt&>(entity);
	std::shared_ptr<RampEnt> clone = Ent::Create<RampEnt>();
	clone->m_rotation = src.m_rotation;
	clone->m_flipped = src.m_flipped;
	clone->m_size = src.m_size;
	clone->m_position = src.m_position;
	return clone;
}
