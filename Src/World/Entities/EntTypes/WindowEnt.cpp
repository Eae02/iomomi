#include "WindowEnt.hpp"
#include "../../Collision.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Protobuf/Build/WindowEntity.pb.h"

#include <imgui.h>
#include <glm/glm.hpp>

static constexpr float BOX_SHAPE_RADIUS = 0.03;

static eg::Model* windowModel;

struct WindowType
{
	const char* name;
	const eg::IMaterial* material;
	bool blocksGravityGun;
	bool blocksWater;
};

static WindowType windowTypes[1];

static void OnInit()
{
	windowTypes[0] = { "Grating", &eg::GetAsset<StaticPropMaterial>("Materials/Platform.yaml"), false, false };
	
	windowModel = &eg::GetAsset<eg::Model>("Models/Window.obj");
}

EG_ON_INIT(OnInit)

WindowEnt::WindowEnt()
{
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
	
	if (ImGui::BeginCombo("Window Type", windowTypes[m_windowType].name))
	{
		for (uint32_t i = 0; i < std::size(windowTypes); i++)
		{
			if (ImGui::Selectable(windowTypes[i].name, m_windowType == i))
			{
				m_windowType = i;
			}
			if (ImGui::IsItemHovered())
			{
				static const char* noYes[] = {"no", "yes"};
				ImGui::SetTooltip("Blocks Water: %s\nBlocks Gun: %s", noYes[windowTypes[i].blocksWater],
				                  noYes[windowTypes[i].blocksGravityGun]);
			}
		}
		ImGui::EndCombo();
	}
}

void WindowEnt::CommonDraw(const EntDrawArgs& args)
{
	auto [tangent, bitangent] = m_aaQuad.GetTangents(0);
	glm::vec3 normal = glm::normalize(glm::cross(tangent, bitangent));
	glm::mat4 transform = glm::translate(glm::mat4(1), m_physicsObject.position) * glm::mat4(
		glm::vec4(tangent * 0.5f, 0),
		glm::vec4(normal, 0),
		glm::vec4(bitangent * 0.5f, 0),
		glm::vec4(0, 0, 0, 1)
	);
	glm::vec2 textureScale = m_aaQuad.radius / m_textureScale;
	args.meshBatch->AddModel(*windowModel, *windowTypes[m_windowType].material,
		StaticPropMaterial::InstanceData(transform, textureScale));
}

const void* WindowEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(AxisAlignedQuadComp))
		return &m_aaQuad;
	if (type == typeid(WaterBlockComp) && windowTypes[m_windowType].blocksWater)
		return &m_waterBlockComp;
	return nullptr;
}

void WindowEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::WindowEntity windowPB;
	
	SerializePos(windowPB, m_physicsObject.position);
	
	windowPB.set_up_plane(m_aaQuad.upPlane);
	windowPB.set_sizex(m_aaQuad.radius.x);
	windowPB.set_sizey(m_aaQuad.radius.y);
	windowPB.set_texture_scale(m_textureScale);
	windowPB.set_window_type(m_windowType);
	
	windowPB.SerializeToOstream(&stream);
}

void WindowEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WindowEntity windowPB;
	windowPB.ParseFromIstream(&stream);
	
	m_physicsObject.position = DeserializePos(windowPB);
	
	m_aaQuad.upPlane = windowPB.up_plane();
	m_aaQuad.radius = glm::vec2(windowPB.sizex(), windowPB.sizey());
	m_textureScale = windowPB.texture_scale();
	m_windowType = windowPB.window_type();
	if (m_windowType >= std::size(windowTypes))
		m_windowType = 0;
	
	if (windowTypes[m_windowType].blocksGravityGun)
	{
		m_physicsObject.rayIntersectMask |= RAY_MASK_BLOCK_GUN;
	}
	
	std::fill_n(m_waterBlockComp.blockedGravities, 6, true);
	m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_physicsObject.position);
	
	auto [tangent, bitangent] = m_aaQuad.GetTangents(0);
	glm::vec3 normal = m_aaQuad.GetNormal() * BOX_SHAPE_RADIUS;
	glm::vec3 boxSize = (tangent + bitangent) * 0.5f + normal;
	
	m_physicsObject.shape = eg::AABB(-normal - boxSize, -normal + boxSize);
}

void WindowEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

void WindowEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_physicsObject.position = newPosition;
}

glm::vec3 WindowEnt::GetPosition() const
{
	return m_physicsObject.position;
}
