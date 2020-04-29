#include "WindowEnt.hpp"
#include "../../../../Protobuf/Build/WindowEntity.pb.h"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../Collision.hpp"

#include <imgui.h>
#include <glm/glm.hpp>

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
	
	m_physicsObject.canCarry = false;
	m_physicsObject.canBePushed = false;
	m_physicsObject.rayIntersectMask = RAY_MASK_BLOCK_PICK_UP;
}

void WindowEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::DragFloat2("Size", &m_aaQuad.radius.x, 0.5f);
	
	ImGui::Combo("Plane", &m_aaQuad.upPlane, "X\0Y\0Z\0");
	
	ImGui::DragFloat("Texture Scale", &m_textureScale, 0.5f, 0.0f, INFINITY);
}

void WindowEnt::CommonDraw(const EntDrawArgs& args)
{
	auto [tangent, bitangent] = m_aaQuad.GetTangents(0);
	glm::vec3 normal = glm::normalize(glm::cross(tangent, bitangent));
	glm::mat4 transform = glm::translate(glm::mat4(1), Pos()) * glm::mat4(
		glm::vec4(tangent * 0.5f, 0),
		glm::vec4(normal, 0),
		glm::vec4(bitangent * 0.5f, 0),
		glm::vec4(0, 0, 0, 1)
	);
	glm::vec2 textureScale = m_aaQuad.radius / m_textureScale;
	args.meshBatch->AddModel(*windowModel, *m_material, StaticPropMaterial::InstanceData(transform, textureScale));
}

const void* WindowEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(AxisAlignedQuadComp))
		return &m_aaQuad;
	return nullptr;
}

void WindowEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::WindowEntity windowPB;
	
	SerializePos(windowPB);
	
	windowPB.set_up_plane(m_aaQuad.upPlane);
	windowPB.set_sizex(m_aaQuad.radius.x);
	windowPB.set_sizey(m_aaQuad.radius.y);
	windowPB.set_texture_scale(m_textureScale);
	
	windowPB.SerializeToOstream(&stream);
}

void WindowEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WindowEntity windowPB;
	windowPB.ParseFromIstream(&stream);
	
	DeserializePos(windowPB);
	
	m_aaQuad.upPlane = windowPB.up_plane();
	m_aaQuad.radius = glm::vec2(windowPB.sizex(), windowPB.sizey());
	m_textureScale = windowPB.texture_scale();
	
	auto [tangent, bitangent] = m_aaQuad.GetTangents(0);
	glm::vec3 normal = m_aaQuad.GetNormal() * BOX_SHAPE_RADIUS;
	glm::vec3 boxSize = (tangent + bitangent) * 0.5f + normal;
	glm::vec3 boxCenter = Pos() - normal;
	
	m_physicsObject.shape = eg::AABB(boxCenter - boxSize, boxCenter + boxSize);
	
	m_collisionGeometry = m_aaQuad.GetCollisionGeometry(boxCenter, BOX_SHAPE_RADIUS);
}

void WindowEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}
