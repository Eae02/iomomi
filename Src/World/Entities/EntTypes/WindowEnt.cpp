#include "WindowEnt.hpp"
#include "../../Collision.hpp"
#include "../../../Graphics/RenderSettings.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Materials/BlurredGlassMaterial.hpp"
#include "../../../../Protobuf/Build/WindowEntity.pb.h"

#include <imgui.h>
#include <glm/glm.hpp>

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

static eg::Buffer windowVertexBuffer;

static const eg::Model* frameModel;
static int frameMeshIndices[3][3];

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
	
	frameModel = &eg::GetAsset<eg::Model>("Models/WindowFrame.obj");
	
	std::fill_n(frameMeshIndices[0], 9, -1);
	frameMeshIndices[0][0] = frameModel->GetMeshIndex("SideBL");
	frameMeshIndices[1][0] = frameModel->GetMeshIndex("SideB");
	frameMeshIndices[2][0] = frameModel->GetMeshIndex("SideBR");
	frameMeshIndices[0][2] = frameModel->GetMeshIndex("SideTL");
	frameMeshIndices[1][2] = frameModel->GetMeshIndex("SideT");
	frameMeshIndices[2][2] = frameModel->GetMeshIndex("SideTR");
	frameMeshIndices[0][1] = frameModel->GetMeshIndex("SideL");
	frameMeshIndices[2][1] = frameModel->GetMeshIndex("SideR");
	
	eg::StdVertex windowVertices[6];
	auto InitVertex = [&] (eg::StdVertex& vertex, int x, int y)
	{
		vertex.position[0] = x * 2 - 1;
		vertex.position[1] = y * 2 - 1;
		vertex.position[2] = 0.5f;
		vertex.normal[0] = 0;
		vertex.normal[1] = 0;
		vertex.normal[2] = 127;
		vertex.normal[3] = 0;
		vertex.tangent[0] = 127;
		vertex.tangent[1] = 0;
		vertex.tangent[2] = 0;
		vertex.tangent[3] = 0;
		vertex.texCoord[0] = x;
		vertex.texCoord[1] = y;
	};
	InitVertex(windowVertices[0], 0, 0);
	InitVertex(windowVertices[1], 1, 0);
	InitVertex(windowVertices[2], 0, 1);
	InitVertex(windowVertices[3], 1, 0);
	InitVertex(windowVertices[4], 1, 1);
	InitVertex(windowVertices[5], 0, 1);
	windowVertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(windowVertices), windowVertices);
}

void OnShutdown()
{
	windowVertexBuffer.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

WindowEnt::WindowEnt()
{
	m_physicsObject.canCarry = false;
	m_physicsObject.canBePushed = false;
	m_physicsObject.rayIntersectMask = RAY_MASK_BLOCK_PICK_UP | RAY_MASK_CLIMB;
}

void WindowEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::DragFloat2("Size", &m_aaQuad.radius.x, 0.5f);
	
	ImGui::Combo("Plane", &m_aaQuad.upPlane, "X\0Y\0Z\0");
	
	ImGui::Combo("Origin", reinterpret_cast<int*>(&m_originMode), "Top\0Middle\0Bottom\0");
	
	ImGui::DragFloat("Depth", &m_depth, 0.05f, 0.0f, INFINITY);
	ImGui::DragFloat("Plane Distance", &m_windowDistanceScale, 0.01f, 0.0f, 1.0f);
	
	ImGui::Separator();
	
	ImGui::DragFloat("Texture Scale", &m_textureScale, 0.5f, 0.0f, INFINITY);
	
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
	
	ImGui::Separator();
	
	ImGui::Checkbox("Has Frame", &m_hasFrame);
	if (ImGui::DragFloat("Frame Scale", &m_frameScale, 0.1f, 0.0f, INFINITY))
		m_frameScale = std::max(m_frameScale, 0.0f);
	
	ImGui::Separator();
	
	static const char* blockWaterStrAutoYes = "Auto (yes)\0Never\0Always\0";
	static const char* blockWaterStrAutoNo = "Auto (no)\0Never\0Always\0";
	if (ImGui::Combo("Block Water", reinterpret_cast<int*>(&m_waterBlockMode),
		windowTypes[m_windowType].blocksWater ? blockWaterStrAutoYes : blockWaterStrAutoNo))
	{
		UpdateWaterBlock();
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
	const eg::IMaterial& material = *windowTypes[m_windowType].material;
	const bool useTransparentMeshBatch = material.GetOrderRequirement() == eg::IMaterial::OrderRequirement::OnlyOrdered;
	if (useTransparentMeshBatch && args.transparentMeshBatch == nullptr)
		return;
	
	auto [tangent, bitangent] = m_aaQuad.GetTangents(0);
	const glm::vec3 normal = m_aaQuad.GetNormal();
	
	glm::mat4 translationMatrix = glm::translate(glm::mat4(1), m_physicsObject.position);
	if (m_originMode == OriginMode::Top)
		translationMatrix = glm::translate(translationMatrix, -normal * m_depth * 0.5f);
	else if (m_originMode == OriginMode::Bottom)
		translationMatrix = glm::translate(translationMatrix, normal * m_depth * 0.5f);
	
	//Draws the frame
	if (m_hasFrame)
	{
		const eg::IMaterial& frameMaterial = eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
		
		const glm::mat4 frameCommonTransform = translationMatrix *
			glm::translate(glm::mat4(), (tangent - bitangent) * 0.5f) *
			glm::mat4(
				glm::vec4(glm::normalize(bitangent) * m_frameScale, 0),
				glm::vec4(glm::normalize(tangent) * m_frameScale, 0),
				glm::vec4(-normal * m_depth, 0),
				glm::vec4(0, 0, 0, 1)
			);
		
		int frameBlocksW = std::max((int)std::round(glm::length(bitangent) / m_frameScale), 2);
		int frameBlocksH = std::max((int)std::round(glm::length(tangent) / m_frameScale), 2);
		auto DrawFrameMesh = [&] (int x, int y)
		{
			int miX = (x == frameBlocksW - 1) ? 2 : std::min(x, 1);
			int miY = (y == frameBlocksH - 1) ? 2 : std::min(y, 1);
			if (frameMeshIndices[miX][miY] == -1)
				return;
			
			glm::mat4 transform = glm::translate(frameCommonTransform, glm::vec3(x - miX, -(y - miY), -0.5f));
			StaticPropMaterial::InstanceData instanceData(transform);
			args.meshBatch->AddModelMesh(*frameModel, frameMeshIndices[miX][miY], frameMaterial, instanceData);
		};
		
		for (int x = 0; x < frameBlocksW; x++)
		{
			DrawFrameMesh(x, 0);
			DrawFrameMesh(x, frameBlocksH - 1);
		}
		for (int y = 1; y < frameBlocksH - 1; y++)
		{
			DrawFrameMesh(0, y);
			DrawFrameMesh(frameBlocksW - 1, y);
		}
	}
	
	const glm::vec2 planeTextureScale = m_aaQuad.radius / m_textureScale;
	auto DrawWindowPlaneWithTransform = [&] (const glm::mat4& transform)
	{
		eg::MeshBatch::Mesh mesh = {};
		mesh.vertexBuffer = windowVertexBuffer;
		mesh.numElements = 6;
		StaticPropMaterial::InstanceData instanceData(transform, planeTextureScale);
		if (useTransparentMeshBatch)
		{
			glm::vec3 centerPos = transform * glm::vec4(0, 0, 0.5f, 1.0f);
			args.transparentMeshBatch->Add(mesh, material, instanceData, DepthDrawOrder(centerPos));
		}
		else
		{
			args.meshBatch->Add(mesh, material, instanceData);
		}
	};
	
	const float planeNormalScale = m_depth * std::min(m_windowDistanceScale, m_hasFrame ? 0.99f : 1.0f);
	
	//Draws the first window side
	const glm::mat4 transformSide1 = translationMatrix * glm::mat4(
		glm::vec4(-tangent * 0.5f, 0),
		glm::vec4(bitangent * 0.5f, 0),
		glm::vec4(normal * planeNormalScale, 0),
		glm::vec4(0, 0, 0, 1)
	);
	DrawWindowPlaneWithTransform(transformSide1);
	
	//If the plane normal scale is too small, only one side should be drawn (is is assumed that the material is two sided)
	if (planeNormalScale > 0.001f)
	{
		const glm::mat4 transformSide2 = translationMatrix * glm::mat4(
			glm::vec4(tangent * 0.5f, 0),
			glm::vec4(bitangent * 0.5f, 0),
			glm::vec4(-normal * planeNormalScale, 0),
			glm::vec4(0, 0, 0, 1)
		);
		DrawWindowPlaneWithTransform(transformSide2);
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
	windowPB.set_depth(m_depth);
	windowPB.set_window_distance_scale(m_windowDistanceScale);
	windowPB.set_has_depth_data(true);
	windowPB.set_border_mesh(m_hasFrame);
	windowPB.set_origin_mode((int)m_originMode);
	windowPB.set_border_scale(m_frameScale);
	
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
	m_hasFrame = windowPB.border_mesh();
	m_originMode = (OriginMode)windowPB.origin_mode();
	if (windowPB.border_scale() != 0)
		m_frameScale = windowPB.border_scale();
	if (m_windowType >= windowTypes.size())
		m_windowType = 0;
	
	if (windowPB.has_depth_data())
	{
		m_depth = windowPB.depth();
		m_windowDistanceScale = windowPB.window_distance_scale();
	}
	
	if (windowTypes[m_windowType].blocksGravityGun)
	{
		m_physicsObject.rayIntersectMask |= RAY_MASK_BLOCK_GUN;
	}
	
	UpdateWaterBlock();
	m_waterBlockComp.InitFromAAQuadComponent(m_aaQuad, m_physicsObject.position);
	
	auto[tangent, bitangent] = m_aaQuad.GetTangents(0);
	const glm::vec3 boxSize = (tangent + bitangent + m_aaQuad.GetNormal() * m_depth) * 0.5f;
	const glm::vec3 shapeCenter = GetTranslationFromOriginMode();
	m_physicsObject.shape = eg::AABB(shapeCenter + boxSize, shapeCenter - boxSize);
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

glm::vec3 WindowEnt::GetEditorGridAlignment() const
{
	glm::vec3 alignment(0.1f);
	alignment[m_aaQuad.upPlane] = 0.05f;
	return alignment;
}

glm::vec3 WindowEnt::GetTranslationFromOriginMode() const
{
	switch (m_originMode)
	{
	case OriginMode::Top:
		return -m_aaQuad.GetNormal() * m_depth * 0.5f;
	case OriginMode::Middle:
		return glm::vec3(0);
	case OriginMode::Bottom:
		return m_aaQuad.GetNormal() * m_depth * 0.5f;
	}
	EG_UNREACHABLE
}
