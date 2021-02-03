#include "LadderEnt.hpp"
#include "../../../../Protobuf/Build/LadderEntity.pb.h"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"

#include <imgui.h>

enum class LadderComponent
{
	Top,
	Middle,
	Bottom
};

static const eg::Model* ladderModel;
static const eg::IMaterial* laddderMaterial;
static std::vector<LadderComponent> meshComponents;

static void OnInit()
{
	ladderModel = &eg::GetAsset<eg::Model>("Models/Ladder.aa.obj");
	meshComponents.resize(ladderModel->NumMeshes());
	for (size_t i = 0; i < ladderModel->NumMeshes(); i++)
	{
		std::string_view meshNamePrefix(ladderModel->GetMesh(i).name.c_str(), 3);
		if (meshNamePrefix == "Top")
			meshComponents[i] = LadderComponent::Top;
		else if (meshNamePrefix == "Mid")
			meshComponents[i] = LadderComponent::Middle;
		else if (meshNamePrefix == "Btm")
			meshComponents[i] = LadderComponent::Bottom;
		else
			EG_PANIC("Unknown mesh prefix")
	}
	
	laddderMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Ladder.yaml");
}

EG_ON_INIT(OnInit)

LadderEnt::LadderEnt()
{
	UpdateTransformAndAABB();
}

void LadderEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	if (ImGui::DragInt("Length", &m_length, 1, 1, INT_MAX, nullptr, ImGuiSliderFlags_AlwaysClamp))
	{
		UpdateTransformAndAABB();
	}
	
	if (ImGui::SliderInt("Down", &m_downDirection, 0, 3, nullptr, ImGuiSliderFlags_AlwaysClamp))
	{
		UpdateTransformAndAABB();
	}
}

void LadderEnt::CommonDraw(const EntDrawArgs& args)
{
	auto DrawPart = [&] (float yOffset, LadderComponent comp)
	{
		StaticPropMaterial::InstanceData instanceData(glm::translate(m_commonTransform, glm::vec3(0, yOffset, 0)));
		for (size_t i = 0; i < meshComponents.size(); i++)
		{
			if (meshComponents[i] == comp)
			{
				args.meshBatch->AddModelMesh(*ladderModel, i, *laddderMaterial, instanceData);
			}
		}
	};
	
	DrawPart(0, LadderComponent::Top);
	for (int i = 0; i < m_length; i++)
	{
		DrawPart(i * -2, LadderComponent::Middle);
	}
	DrawPart(m_length * -2 + 2, LadderComponent::Bottom);
}

void LadderEnt::UpdateTransformAndAABB()
{
	const glm::vec3 xDir = DirectionVector(m_forward);
	const glm::vec3 zDir = voxel::tangents[(int)m_forward];
	
	m_commonTransform =
		glm::translate(glm::mat4(1), m_position) *
		glm::mat4(glm::mat3(xDir, glm::cross(zDir, xDir), zDir)) *
		glm::rotate(glm::mat4(), eg::HALF_PI * m_downDirection, glm::vec3(1, 0, 0)) *
		glm::scale(glm::mat4(), glm::vec3(0.5f));
	
	constexpr float DEPTH = 0.25f;
	m_aabb = eg::AABB(glm::vec3(-DEPTH, m_length * -2 - 1, -0.5f), glm::vec3(DEPTH, 1, 0.5f))
		.TransformedBoundingBox(m_commonTransform);
	
	m_downVector = glm::normalize(glm::vec3(m_commonTransform * glm::vec4(0, -1, 0, 0)));
}

void LadderEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	if (faceDirection)
		m_forward = *faceDirection;
	UpdateTransformAndAABB();
}

void LadderEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::LadderEntity entityPB;
	
	entityPB.set_dir((gravity_pb::Dir)m_forward);
	SerializePos(entityPB, m_position);
	
	entityPB.set_length(m_length);
	entityPB.set_down(m_downDirection);
	
	entityPB.SerializeToOstream(&stream);
}

void LadderEnt::Deserialize(std::istream& stream)
{
	gravity_pb::LadderEntity entityPB;
	entityPB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(entityPB);
	m_forward = (Dir)entityPB.dir();
	m_length = entityPB.length();
	m_downDirection = entityPB.down();
	
	UpdateTransformAndAABB();
}

glm::vec3 LadderEnt::GetEditorGridAlignment() const
{
	return glm::vec3(0.5f);
}
