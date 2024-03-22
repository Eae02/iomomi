#include "LadderEnt.hpp"

#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../ImGui.hpp"
#include <LadderEntity.pb.h>

DEF_ENT_TYPE(LadderEnt)

enum class LadderComponent
{
	Top,
	Middle,
	Bottom
};

static const eg::Model* ladderModel;
static const eg::IMaterial* laddderMaterial;
static std::vector<eg::CollisionMesh> ladderCollisionMeshes;
static std::vector<LadderComponent> meshComponents;

static void OnInit()
{
	ladderModel = &eg::GetAsset<eg::Model>("Models/Ladder.aa.obj");
	meshComponents.resize(ladderModel->NumMeshes());
	ladderCollisionMeshes.reserve(ladderModel->NumMeshes());
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
		ladderCollisionMeshes.emplace_back(ladderModel->MakeCollisionMesh(i).value());
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
#ifdef EG_HAS_IMGUI
	Ent::RenderSettings();

	if (ImGui::DragInt("Length", &m_length, 0.1f, 1, INT_MAX, nullptr, ImGuiSliderFlags_AlwaysClamp))
	{
		UpdateTransformAndAABB();
	}

	if (ImGui::SliderInt("Down", &m_downDirection, 0, 3, nullptr, ImGuiSliderFlags_AlwaysClamp))
	{
		UpdateTransformAndAABB();
	}
#endif
}

template <typename CallbackTp>
void LadderEnt::IterateParts(CallbackTp callback)
{
	callback(m_commonTransform, LadderComponent::Top);
	for (int i = 0; i < m_length; i++)
	{
		callback(glm::translate(m_commonTransform, glm::vec3(0, i * -2, 0)), LadderComponent::Middle);
	}
	callback(glm::translate(m_commonTransform, glm::vec3(0, m_length * -2 + 2, 0)), LadderComponent::Bottom);
}

void LadderEnt::CommonDraw(const EntDrawArgs& args)
{
	IterateParts(
		[&](const glm::mat4& transform, LadderComponent comp)
		{
			StaticPropMaterial::InstanceData instanceData(transform);
			for (size_t i = 0; i < meshComponents.size(); i++)
			{
				if (meshComponents[i] == comp)
				{
					args.meshBatch->AddModelMesh(*ladderModel, i, *laddderMaterial, instanceData);
				}
			}
		}
	);
}

void LadderEnt::UpdateTransformAndAABB()
{
	const glm::vec3 xDir = DirectionVector(m_forward);
	const glm::vec3 zDir = voxel::tangents[static_cast<int>(m_forward)];

	m_commonTransform = glm::translate(glm::mat4(1), m_position) *
	                    glm::mat4(glm::mat3(xDir, glm::cross(zDir, xDir), zDir)) *
	                    glm::rotate(glm::mat4(), eg::HALF_PI * m_downDirection, glm::vec3(1, 0, 0)) *
	                    glm::scale(glm::mat4(), glm::vec3(0.5f));

	constexpr float DEPTH = 0.25f;
	m_aabb = eg::AABB(glm::vec3(-DEPTH, m_length * -2 - 1, -0.5f), glm::vec3(DEPTH, 1, 0.5f))
	             .TransformedBoundingBox(m_commonTransform);

	m_downVector = glm::normalize(glm::vec3(m_commonTransform * glm::vec4(0, -1, 0, 0)));

	m_editorSelectionMeshes.clear();
	IterateParts(
		[&](const glm::mat4& transform, LadderComponent comp)
		{
			for (size_t i = 0; i < meshComponents.size(); i++)
			{
				if (meshComponents[i] == comp)
				{
					EditorSelectionMesh& selectionMesh = m_editorSelectionMeshes.emplace_back();
					selectionMesh.transform = transform;
					selectionMesh.meshIndex = i;
					selectionMesh.model = ladderModel;
					selectionMesh.collisionMesh = &ladderCollisionMeshes[i];
				}
			}
		}
	);
}

void LadderEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	if (faceDirection)
		m_forward = *faceDirection;
	UpdateTransformAndAABB();
}

std::span<const EditorSelectionMesh> LadderEnt::EdGetSelectionMeshes() const
{
	return m_editorSelectionMeshes;
}

void LadderEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::LadderEntity entityPB;

	entityPB.set_dir(static_cast<iomomi_pb::Dir>(m_forward));
	SerializePos(entityPB, m_position);

	entityPB.set_length(m_length);
	entityPB.set_down(m_downDirection);

	entityPB.SerializeToOstream(&stream);
}

void LadderEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::LadderEntity entityPB;
	entityPB.ParseFromIstream(&stream);

	m_position = DeserializePos(entityPB);
	m_forward = static_cast<Dir>(entityPB.dir());
	m_length = entityPB.length();
	m_downDirection = entityPB.down();

	UpdateTransformAndAABB();
}

glm::vec3 LadderEnt::EdGetGridAlignment() const
{
	return glm::vec3(0.5f);
}
