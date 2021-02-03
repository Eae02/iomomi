#include "MeshEnt.hpp"
#include "../../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../../Protobuf/Build/MeshEntity.pb.h"
#include "../../../../ImGuiInterface.hpp"
#include "../../../../Game.hpp"

#include <imgui.h>

struct ModelOption
{
	const char* name;
	const char* serializedName;
	const eg::Model* model;
	int meshIndex;
	int repeatAxis = -1;
	float repeatDistance = 0;
	glm::vec2 repeatUVShift;
	std::vector<std::pair<const char*, const eg::IMaterial*>> materialOptions;
	
	bool blockGun = false;
	bool useMeshAABBForCollision = false;
	std::optional<eg::CollisionMesh> collisionMesh;
	
	eg::CollisionMesh selectionMesh;
	
	ModelOption(std::string_view modelName, std::string_view meshName, std::initializer_list<const char*> materialOptionNames)
		: model(&eg::GetAsset<eg::Model>(modelName)),
		  materialOptions(materialOptionNames.size())
	{
		std::transform(materialOptionNames.begin(), materialOptionNames.end(), materialOptions.begin(),
		               [&] (const char* materialName) -> std::pair<const char*, const eg::IMaterial*>
		{
			return { materialName, &eg::GetAsset<StaticPropMaterial>(materialName) };
		});
		
		if (!meshName.empty())
		{
			meshIndex = model->GetMeshIndex(meshName);
			EG_ASSERT(meshIndex != -1);
			selectionMesh = model->MakeCollisionMesh(meshIndex);
		}
		else
		{
			meshIndex = -1;
			selectionMesh = model->MakeCollisionMesh();
		}
	}
	
	void SetCollisionMesh(std::string_view collisionMeshName)
	{
		collisionMesh = eg::GetAsset<eg::Model>(collisionMeshName).MakeCollisionMesh(0);
	}
};

std::vector<ModelOption> modelOptions;

static void OnInit()
{
	ModelOption railingCenter("Models/Railing.aa.obj", "center", {"Materials/Railing.yaml" });
	railingCenter.name = "Railing";
	railingCenter.serializedName = "RailingCenter";
	railingCenter.SetCollisionMesh("Models/Railing.col.obj");
	modelOptions.push_back(std::move(railingCenter));
	
	ModelOption railingCorner("Models/Railing.aa.obj", "edge", {"Materials/Railing.yaml" });
	railingCorner.name = "Railing (corner)";
	railingCorner.serializedName = "RailingCorner";
	modelOptions.push_back(std::move(railingCorner));
	
	ModelOption pipeStraight("Models/Pipe.aa.obj", "straight", {"Materials/PipeCenter.yaml" });
	pipeStraight.name = "Pipe (straight)";
	pipeStraight.serializedName = "PipeS";
	pipeStraight.repeatAxis = 1;
	pipeStraight.repeatDistance = 1;
	pipeStraight.repeatUVShift = { 0.32f, 0.0f };
	pipeStraight.blockGun = true;
	pipeStraight.useMeshAABBForCollision = true;
	modelOptions.push_back(std::move(pipeStraight));
	
	ModelOption pipeBend("Models/Pipe.aa.obj", "bend", {"Materials/PipeCenter.yaml" });
	pipeBend.name = "Pipe (bend)";
	pipeBend.serializedName = "PipeB";
	pipeBend.blockGun = true;
	pipeBend.useMeshAABBForCollision = true;
	modelOptions.push_back(std::move(pipeBend));
	
	ModelOption pipeRing("Models/Pipe.aa.obj", "connection", {"Materials/Default.yaml" });
	pipeRing.name = "Pipe (ring)";
	pipeRing.serializedName = "PipeR";
	modelOptions.push_back(std::move(pipeRing));
}

EG_ON_INIT(OnInit)

MeshEnt::MeshEnt()
{
	m_scale = glm::vec3(1);
	
	m_physicsObject.canBePushed = false;
	m_physicsObject.debugColor = 0x12b81a;
	m_randomTextureOffset = std::uniform_real_distribution<float>(0, 10000)(globalRNG);
}

void MeshEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	if (ImGui::DragFloat3("Scale", &m_scale.x, 0.1f))
	{
		UpdateEditorSelectionMeshes();
	}
	
	if (ImGui::Button("Reset Rotation"))
	{
		m_rotation = glm::quat();
		UpdateEditorSelectionMeshes();
	}
	
	ImGui::Separator();
	
	const char* currentModelName = m_model.has_value() ? modelOptions[*m_model].name : nullptr;
	if (ImGui::BeginCombo("Model", currentModelName))
	{
		for (uint32_t i = 0; i < modelOptions.size(); i++)
		{
			if (ImGui::Selectable(modelOptions[i].name, m_model == i))
			{
				m_model = i;
				if (m_material >= modelOptions[i].materialOptions.size())
					m_material = 0;
				if (modelOptions[i].repeatAxis == -1)
					m_numRepeats = 0;
				UpdateEditorSelectionMeshes();
			}
		}
		ImGui::EndCombo();
	}
	
	const bool materialDisabled = !m_model.has_value() || modelOptions[*m_model].materialOptions.size() == 1;
	const char* currentMaterialName = m_model.has_value() ? modelOptions[*m_model].materialOptions[m_material].first : nullptr;
	ImPushDisabled(materialDisabled);
	if (ImGui::BeginCombo("Material", currentMaterialName))
	{
		for (uint32_t i = 0; i < modelOptions[*m_model].materialOptions.size(); i++)
		{
			if (ImGui::Selectable(modelOptions[*m_model].materialOptions[i].first, m_material == i))
			{
				m_material = i;
			}
		}
		ImGui::EndCombo();
	}
	ImPopDisabled(materialDisabled);
	
	const bool repeatDisabled = !m_model.has_value() || modelOptions[*m_model].repeatAxis == -1;
	ImPushDisabled(repeatDisabled);
	if (ImGui::DragInt("Repeats", &m_numRepeats, 0.1f, -100, 100, nullptr, ImGuiSliderFlags_AlwaysClamp))
	{
		UpdateEditorSelectionMeshes();
	}
	ImPopDisabled(repeatDisabled);
}

template <typename CallbackFn>
void MeshEnt::IterateRepeatedInstances(CallbackFn callback) const
{
	if (!m_model.has_value())
		return;
	
	glm::mat4 commonTransform =
		glm::translate(glm::mat4(1), m_position) *
		glm::mat4_cast(m_rotation) *
		glm::scale(glm::mat4(1), m_scale);
	
	for (int i = std::min(m_numRepeats, 0); i <= std::max(m_numRepeats, 0); i++)
	{
		glm::mat4 transform = commonTransform;
		glm::vec2 textureMin(0, 0);
		if (modelOptions[*m_model].repeatAxis != -1)
		{
			glm::vec3 translation(0);
			translation[modelOptions[*m_model].repeatAxis] = modelOptions[*m_model].repeatDistance * i;
			transform = glm::translate(transform, translation);
			textureMin = modelOptions[*m_model].repeatUVShift * (m_randomTextureOffset + (float)i);
		}
		
		callback(transform, textureMin);
	}
}

void MeshEnt::CommonDraw(const EntDrawArgs& args)
{
	IterateRepeatedInstances([&] (const glm::mat4& transform, const glm::vec2& textureMin)
	{
		StaticPropMaterial::InstanceData instanceData(transform, textureMin, textureMin + 1.0f);
		
		for (size_t m = 0; m < modelOptions[*m_model].model->NumMeshes(); m++)
		{
			if (modelOptions[*m_model].meshIndex == -1 || (size_t)modelOptions[*m_model].meshIndex == m)
			{
				eg::AABB aabb = modelOptions[*m_model].model->GetMesh(m).boundingAABB->TransformedBoundingBox(transform);
				if (args.frustum == nullptr || args.frustum->Intersects(aabb))
				{
					args.meshBatch->AddModelMesh(
						*modelOptions[*m_model].model, m,
						*modelOptions[*m_model].materialOptions[m_material].second,
						instanceData
					);
				}
			}
		}
	});
}

void MeshEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	UpdateEditorSelectionMeshes();
}

glm::quat MeshEnt::GetEditorRotation()
{
	return m_rotation;
}

void MeshEnt::EditorRotated(const glm::quat& newRotation)
{
	m_rotation = newRotation;
	UpdateEditorSelectionMeshes();
}

void MeshEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::MeshEntity entityPB;
	
	SerializePos(entityPB, m_position);
	entityPB.set_scalex(m_scale.x);
	entityPB.set_scaley(m_scale.y);
	entityPB.set_scalez(m_scale.z);
	entityPB.set_rotx(m_rotation.x);
	entityPB.set_roty(m_rotation.y);
	entityPB.set_rotz(m_rotation.z);
	entityPB.set_rotw(m_rotation.w);
	
	entityPB.set_model_name(m_model.has_value() ? modelOptions[*m_model].serializedName : "");
	entityPB.set_material_index(m_material);
	entityPB.set_num_repeats(m_numRepeats);
	
	entityPB.SerializeToOstream(&stream);
}

void MeshEnt::Deserialize(std::istream& stream)
{
	gravity_pb::MeshEntity entityPB;
	entityPB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(entityPB);
	m_scale.x = entityPB.scalex();
	m_scale.y = entityPB.scaley();
	m_scale.z = entityPB.scalez();
	m_rotation.x = entityPB.rotx();
	m_rotation.y = entityPB.roty();
	m_rotation.z = entityPB.rotz();
	m_rotation.w = entityPB.rotw();
	m_numRepeats = entityPB.num_repeats();
	
	m_model = {};
	m_material = 0;
	for (uint32_t i = 0; i < modelOptions.size(); i++)
	{
		if (std::strcmp(modelOptions[i].serializedName, entityPB.model_name().c_str()) == 0)
		{
			m_model = i;
			m_material = entityPB.material_index();
			if (m_material >= modelOptions[i].materialOptions.size())
				m_material = 0;
			if (modelOptions[i].repeatAxis == -1)
				m_numRepeats = 0;
			break;
		}
	}
	
	if (m_model.has_value())
	{
		const ModelOption& model = modelOptions[*m_model];
		
		m_physicsObject.position = m_position;
		m_physicsObject.rotation = m_rotation;
		m_physicsObject.rayIntersectMask = model.blockGun ? RAY_MASK_BLOCK_GUN : 0;
		
		if (model.collisionMesh.has_value())
		{
			m_physicsObject.shape = &*model.collisionMesh;
			m_hasCollision = true;
		}
		else if (model.useMeshAABBForCollision && model.meshIndex != -1)
		{
			eg::AABB aabb = *model.model->GetMesh(model.meshIndex).boundingAABB;
			
			if (model.repeatAxis != -1)
			{
				glm::vec3 repeatDir(0);
				repeatDir[model.repeatAxis] = model.repeatDistance * m_numRepeats;
				aabb.min = glm::min(aabb.min, aabb.min + repeatDir);
				aabb.max = glm::max(aabb.max, aabb.max + repeatDir);
			}
			
			aabb.min *= m_scale;
			aabb.max *= m_scale;
			
			m_physicsObject.shape = aabb;
			m_hasCollision = true;
		}
	}
	
	UpdateEditorSelectionMeshes();
}

void MeshEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	if (m_hasCollision)
	{
		physicsEngine.RegisterObject(&m_physicsObject);
	}
}

eg::Span<const EditorSelectionMesh> MeshEnt::GetEditorSelectionMeshes() const
{
	return { m_editorSelectionMeshes.data(), m_editorSelectionMeshes.size() };
}

void MeshEnt::UpdateEditorSelectionMeshes()
{
	m_editorSelectionMeshes.clear();
	IterateRepeatedInstances([&] (const glm::mat4& transform, const glm::vec2&)
	{
		EditorSelectionMesh& selectionMesh = m_editorSelectionMeshes.emplace_back();
		selectionMesh.meshIndex = modelOptions[*m_model].meshIndex;
		selectionMesh.model = modelOptions[*m_model].model;
		selectionMesh.collisionMesh = &modelOptions[*m_model].selectionMesh;
		selectionMesh.transform = transform;
	});
}
