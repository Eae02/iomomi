#include "WindowEnt.hpp"
#include "../../Collision.hpp"
#include "../../../Graphics/RenderSettings.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Graphics/Materials/BlurredGlassMaterial.hpp"
#include "../../../../Protobuf/Build/WindowEntity.pb.h"
#include "../../../Graphics/WallShader.hpp"

#include <imgui.h>
#include <glm/glm.hpp>
#include <imgui_internal.h>

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
static int frameFrontMeshIndices[3][3];
static int frameSideMeshIndices[3][3];

struct FrameMaterial
{
	const char* name;
	const StaticPropMaterial* material;
	
	FrameMaterial(const char* _name, const StaticPropMaterial* _material)
		: name(_name), material(_material) { }
};
static std::vector<FrameMaterial> frameMaterials;
static bool frameMaterialsInitialized = false;

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
	
	auto InitMeshIndices = [&] (int x, int y, const char* suffix)
	{
		char nameBuffer[64];
		snprintf(nameBuffer, sizeof(nameBuffer), "Side%s", suffix);
		frameSideMeshIndices[x][y] = frameModel->GetMeshIndex(nameBuffer);
		snprintf(nameBuffer, sizeof(nameBuffer), "Front%s", suffix);
		frameFrontMeshIndices[x][y] = frameModel->GetMeshIndex(nameBuffer);
	};
	InitMeshIndices(0, 0, "BL");
	InitMeshIndices(1, 0, "B");
	InitMeshIndices(2, 0, "BR");
	InitMeshIndices(0, 2, "TL");
	InitMeshIndices(1, 2, "T");
	InitMeshIndices(2, 2, "TR");
	InitMeshIndices(0, 1, "L");
	InitMeshIndices(2, 1, "R");
	
	//Initializes the window plane vertex buffer
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

static void InitializeFrameMaterials()
{
	for (uint32_t i = 1; i < MAX_WALL_MATERIALS; i++)
	{
		if (wallMaterials[i].initialized)
		{
			frameMaterials.emplace_back(wallMaterials[i].name, &StaticPropMaterial::GetFromWallMaterial(i));
		}
	}
	
	frameMaterialsInitialized = true;
}

void OnShutdown()
{
	windowVertexBuffer.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

WindowEnt::WindowEnt()
{
	if (!frameMaterialsInitialized)
	{
		InitializeFrameMaterials();
	}
	
	m_physicsObject.canCarry = false;
	m_physicsObject.canBePushed = false;
	m_physicsObject.rayIntersectMask = RAY_MASK_BLOCK_PICK_UP | RAY_MASK_CLIMB;
	m_frameTextureScale = glm::vec2(1.0f);
	m_textureScale = glm::vec2(1.0f);
}

void WindowEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::DragFloat2("Size", &m_aaQuad.radius.x, 0.5f);
	
	ImGui::Combo("Plane", &m_aaQuad.upPlane, "X\0Y\0Z\0");
	
	ImGui::Combo("Origin", reinterpret_cast<int*>(&m_originMode), "Top\0Middle\0Bottom\0");
	
	ImGui::DragFloat("Depth", &m_depth, 0.05f, 0.0f, INFINITY, nullptr, ImGuiSliderFlags_AlwaysClamp);
	
	ImGui::DragFloat("Plane Distance", &m_windowDistanceScale, 0.01f, 0.0f, 1.0f, nullptr, ImGuiSliderFlags_AlwaysClamp);
	
	ImGui::Separator();
	
	ImGui::DragFloat2("Texture Scale", &m_textureScale.x, 0.5f, 0.01f, INFINITY, nullptr, ImGuiSliderFlags_AlwaysClamp);
	
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
	if (!m_hasFrame)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
	}
	
	ImGui::DragFloat("Frame Scale", &m_frameScale, 0.1f, 0.0f, INFINITY, nullptr, ImGuiSliderFlags_AlwaysClamp);
	ImGui::DragFloat2("Frame Texture Scale", &m_frameTextureScale.x, 0.5f, -INFINITY, INFINITY);
	
	if (ImGui::BeginCombo("Frame Material", frameMaterials[m_frameMaterial].name))
	{
		for (uint32_t material = 0; material < frameMaterials.size(); material++)
		{
			if (ImGui::Selectable(frameMaterials[material].name, m_frameMaterial == material))
			{
				m_frameMaterial = material;
			}
		}
		ImGui::EndCombo();
	}
	
	if (!m_hasFrame)
	{
		ImGui::PopStyleVar();
		ImGui::PopItemFlag();
	}
	
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
		const StaticPropMaterial& frameMaterial = *frameMaterials[m_frameMaterial].material;
		
		const glm::mat4 frameCommonTransform = translationMatrix *
			glm::translate(glm::mat4(), (tangent - bitangent) * 0.5f) *
			glm::mat4(
				glm::vec4(glm::normalize(bitangent) * m_frameScale, 0),
				glm::vec4(glm::normalize(tangent) * m_frameScale, 0),
				glm::vec4(-normal * m_depth, 0),
				glm::vec4(0, 0, 0, 1)
			);
		
		const glm::vec2 MASTER_FRONT_TEXTURE_SCALE(3.0f, -3.0f);
		glm::vec2 frontTextureScale = MASTER_FRONT_TEXTURE_SCALE / m_frameTextureScale;
		glm::vec2 sideTextureScale = glm::vec2(m_depth / m_frameScale, 1) / m_frameTextureScale;
		
		int frameBlocksW = std::max((int)std::round(glm::length(bitangent) / m_frameScale), 2);
		int frameBlocksH = std::max((int)std::round(glm::length(tangent) / m_frameScale), 2);
		auto DrawFrameMesh = [&] (int x, int y)
		{
			int miX = (x == frameBlocksW - 1) ? 2 : std::min(x, 1);
			int miY = (y == frameBlocksH - 1) ? 2 : std::min(y, 1);
			if (miX == 1 && miY == 1)
				return;
			const glm::mat4 transform = glm::translate(frameCommonTransform, glm::vec3(x - miX, -(y - miY), -0.5f));
			args.meshBatch->AddModelMesh(*frameModel, frameFrontMeshIndices[miX][miY], frameMaterial,
			                             StaticPropMaterial::InstanceData(transform, frontTextureScale));
			args.meshBatch->AddModelMesh(*frameModel, frameSideMeshIndices[miX][miY], frameMaterial,
			                             StaticPropMaterial::InstanceData(transform, sideTextureScale));
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
	windowPB.set_texture_scale_x(m_textureScale.x);
	windowPB.set_texture_scale_y(m_textureScale.y);
	windowPB.set_window_type(m_windowType);
	windowPB.set_water_block_mode((int)m_waterBlockMode);
	windowPB.set_depth(m_depth);
	windowPB.set_window_distance_scale(m_windowDistanceScale);
	windowPB.set_has_depth_data(true);
	windowPB.set_border_mesh(m_hasFrame);
	windowPB.set_origin_mode((int)m_originMode);
	windowPB.set_frame_scale(m_frameScale);
	windowPB.set_frame_texture_scale_x(m_frameTextureScale.x);
	windowPB.set_frame_texture_scale_y(m_frameTextureScale.y);
	windowPB.set_frame_material(m_frameMaterial);
	
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
	
	m_textureScale.x = windowPB.texture_scale_x();
	if (windowPB.texture_scale_y() != 0)
		m_textureScale.y = windowPB.texture_scale_y();
	else
		m_textureScale.y = m_textureScale.x;
	
	m_windowType = windowPB.window_type();
	m_hasFrame = windowPB.border_mesh();
	m_originMode = (OriginMode)windowPB.origin_mode();
	if (windowPB.frame_scale() != 0)
		m_frameScale = windowPB.frame_scale();
	
	if (windowPB.frame_texture_scale_x() != 0)
		m_frameTextureScale.x = windowPB.frame_texture_scale_x();
	if (windowPB.frame_texture_scale_y() != 0)
		m_frameTextureScale.y = windowPB.frame_texture_scale_x();
	else
		m_frameTextureScale.y = m_frameTextureScale.x;
	
	m_frameMaterial = windowPB.frame_material();
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
