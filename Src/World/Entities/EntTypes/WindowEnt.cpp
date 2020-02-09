#include "WindowEnt.hpp"
#include "../../../../Protobuf/Build/WindowEntity.pb.h"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"

#include <imgui.h>

static constexpr float BOX_SHAPE_RADIUS = 0.03;

static eg::Model* windowModel;

static void OnInit()
{
	windowModel = &eg::GetAsset<eg::Model>("Models/Window.obj");
}

EG_ON_INIT(OnInit)

WindowEnt::WindowEnt()
{
	m_material = &eg::GetAsset<StaticPropMaterial>("Materials/Platform.yaml");
}

void WindowEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::DragFloat2("Size", &m_aaQuad.size.x, 0.5f);
	
	ImGui::Combo("Plane", &m_aaQuad.upPlane, "X\0Y\0Z\0");
	
	ImGui::DragFloat("Texture Scale", &m_textureScale, 0.5f, 0.0f, INFINITY);
}

void WindowEnt::Draw(eg::MeshBatch& meshBatch, eg::MeshBatchOrdered& transparentMeshBatch) const
{
	auto [tangent, bitangent] = m_aaQuad.GetTangents(0);
	glm::vec3 normal = glm::normalize(glm::cross(tangent, bitangent));
	glm::mat4 transform = glm::translate(glm::mat4(1), Pos()) * glm::mat4(
		glm::vec4(tangent * 0.5f, 0),
		glm::vec4(normal, 0),
		glm::vec4(bitangent * 0.5f, 0),
		glm::vec4(0, 0, 0, 1)
	);
	glm::vec2 textureScale = m_aaQuad.size / m_textureScale;
	meshBatch.AddModel(*windowModel, *m_material, StaticPropMaterial::InstanceData(transform, textureScale));
}

void WindowEnt::Draw(const EntDrawArgs& args)
{
	Draw(*args.meshBatch, *args.transparentMeshBatch);
}

void WindowEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	Draw(*args.meshBatch, *args.transparentMeshBatch);
}

const void* WindowEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(AxisAlignedQuadComp))
		return &m_aaQuad;
	if (type == typeid(RigidBodyComp))
		return &m_rigidBodyComp;
	return nullptr;
}

void WindowEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::WindowEntity windowPB;
	
	SerializePos(windowPB);
	
	windowPB.set_up_plane(m_aaQuad.upPlane);
	windowPB.set_sizex(m_aaQuad.size.x);
	windowPB.set_sizey(m_aaQuad.size.y);
	windowPB.set_texture_scale(m_textureScale);
	
	windowPB.SerializeToOstream(&stream);
}

void WindowEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WindowEntity windowPB;
	windowPB.ParseFromIstream(&stream);
	
	DeserializePos(windowPB);
	
	m_aaQuad.upPlane = windowPB.up_plane();
	m_aaQuad.size = glm::vec2(windowPB.sizex(), windowPB.sizey());
	m_textureScale = windowPB.texture_scale();
	
	auto [tangent, bitangent] = m_aaQuad.GetTangents(0);
	glm::vec3 normal = m_aaQuad.GetNormal() * BOX_SHAPE_RADIUS;
	glm::vec3 boxSize = (tangent + bitangent) * 0.5f + normal;
	m_bulletShape = std::make_unique<btBoxShape>(bullet::FromGLM(boxSize));
	m_rigidBodyComp.InitStatic(this, *m_bulletShape);
	m_rigidBodyComp.SetTransform(Pos() - normal, glm::quat());
}
