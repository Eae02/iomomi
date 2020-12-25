#include "WindowEnt.hpp"
#include "../../Collision.hpp"
#include "../../../Graphics/RenderSettings.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Materials/BlurredGlassMaterial.hpp"
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
	bool needsBlurredTextures = false;
};

static std::array<WindowType, 4> windowTypes;

static int windowTypeDisplayOrder[] = { 0, 3, 1, 2 };
static_assert(std::size(windowTypeDisplayOrder) == windowTypes.size());

BlurredGlassMaterial blurryGlassMaterial;
BlurredGlassMaterial clearGlassMaterial;

static void OnInit()
{
	blurryGlassMaterial.color = clearGlassMaterial.color =
		eg::ColorLin(eg::ColorSRGB::FromHex(0xbed6eb).ScaleAlpha(0.5f));
	
	blurryGlassMaterial.isBlurry = true;
	clearGlassMaterial.isBlurry = false;
	
	windowTypes[0] = { "Grating", &eg::GetAsset<StaticPropMaterial>("Materials/Platform.yaml"), false, false };
	windowTypes[1] = { "Blurred Glass", &blurryGlassMaterial, true, true, true };
	windowTypes[2] = { "Clear Glass", &clearGlassMaterial, true, true };
	windowTypes[3] = { "Grating 2", &eg::GetAsset<StaticPropMaterial>("Materials/Grating2.yaml"), false, false };
	
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
	
	static const char* blockWaterStrAutoYes = "Auto (yes)\0Never\0Always\0";
	static const char* blockWaterStrAutoNo = "Auto (no)\0Never\0Always\0";
	if (ImGui::Combo("Block Water", reinterpret_cast<int*>(&m_waterBlockMode),
		windowTypes[m_windowType].blocksWater ? blockWaterStrAutoYes : blockWaterStrAutoNo))
	{
		UpdateWaterBlock();
	}
	
	if (ImGui::BeginCombo("Window Type", windowTypes[m_windowType].name))
	{
		for (int i : windowTypeDisplayOrder)
		{
			if (ImGui::Selectable(windowTypes[i].name, (int)m_windowType == i))
			{
				m_windowType = i;
				UpdateWaterBlock();
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

void WindowEnt::UpdateWaterBlock()
{
	m_waterBlockComp.editorVersion++;
	bool shouldBlock = windowTypes[m_windowType].blocksWater;
	if (m_waterBlockMode == WaterBlockMode::Never)
		shouldBlock = false;
	if (m_waterBlockMode == WaterBlockMode::Always)
		shouldBlock = true;
	std::fill_n(m_waterBlockComp.blockedGravities, 6, shouldBlock);
}

bool WindowEnt::NeedsBlurredTextures() const
{
	return windowTypes[m_windowType].needsBlurredTextures;
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
	
	StaticPropMaterial::InstanceData instanceData(transform, textureScale);
	
	if (windowTypes[m_windowType].material->GetOrderRequirement() == eg::IMaterial::OrderRequirement::OnlyOrdered)
	{
		if (args.transparentMeshBatch)
		{
			args.transparentMeshBatch->AddModel(*windowModel, *windowTypes[m_windowType].material, instanceData,
			                                    DepthDrawOrder(glm::vec3(transform[3])));
		}
	}
	else
	{
		args.meshBatch->AddModel(*windowModel, *windowTypes[m_windowType].material, instanceData);
	}
}

const void* WindowEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(AxisAlignedQuadComp))
		return &m_aaQuad;
	if (type == typeid(WaterBlockComp))
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
	windowPB.set_water_block_mode((int)m_waterBlockMode);
	
	windowPB.SerializeToOstream(&stream);
}

void WindowEnt::Deserialize(std::istream& stream)
{
	gravity_pb::WindowEntity windowPB;
	windowPB.ParseFromIstream(&stream);
	
	m_physicsObject.position = DeserializePos(windowPB);
	
	m_aaQuad.upPlane = windowPB.up_plane();
	m_aaQuad.radius = glm::vec2(windowPB.sizex(), windowPB.sizey());
	m_waterBlockMode = (WaterBlockMode)windowPB.water_block_mode();
	m_textureScale = windowPB.texture_scale();
	m_windowType = windowPB.window_type();
	if (m_windowType >= windowTypes.size())
		m_windowType = 0;
	
	if (windowTypes[m_windowType].blocksGravityGun)
	{
		m_physicsObject.rayIntersectMask |= RAY_MASK_BLOCK_GUN;
	}
	
	UpdateWaterBlock();
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
	m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_physicsObject.position);
	m_waterBlockComp.editorVersion++;
}

glm::vec3 WindowEnt::GetPosition() const
{
	return m_physicsObject.position;
}
