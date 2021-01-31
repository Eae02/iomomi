#include "MeshEnt.hpp"
#include "../../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../../Protobuf/Build/MeshEntity.pb.h"
#include "../../../../ImGuiInterface.hpp"

#include <imgui.h>

struct ModelOption
{
	const char* name;
	const char* serializedName;
	const eg::Model* model;
	std::optional<eg::CollisionMesh> collisionMesh;
	int meshIndex = -1;
	std::vector<std::pair<const char*, const eg::IMaterial*>> materialOptions;
	bool blockGun = false;
	
	ModelOption(std::string_view modelName, std::initializer_list<const char*> materialOptionNames)
		: model(&eg::GetAsset<eg::Model>(modelName)),
		  materialOptions(materialOptionNames.size())
	{
		std::transform(materialOptionNames.begin(), materialOptionNames.end(), materialOptions.begin(),
		               [&] (const char* materialName) -> std::pair<const char*, const eg::IMaterial*>
		{
			return { materialName, &eg::GetAsset<StaticPropMaterial>(materialName) };
		});
	}
	
	void SetCollisionMesh(std::string_view collisionMeshName)
	{
		collisionMesh = eg::GetAsset<eg::Model>(collisionMeshName).MakeCollisionMesh(0);
	}
	
	void SetMeshIndex(std::string_view meshName)
	{
		meshIndex = model->GetMeshIndex(meshName);
	}
};

std::vector<ModelOption> modelOptions;

static void OnInit()
{
	ModelOption railingCenter("Models/Railing.obj", {"Materials/Railing.yaml" });
	railingCenter.name = "Railing";
	railingCenter.serializedName = "RailingCenter";
	railingCenter.SetMeshIndex("center");
	railingCenter.SetCollisionMesh("Models/RailingCol.obj");
	modelOptions.push_back(std::move(railingCenter));
	
	ModelOption railingCorner("Models/Railing.obj", {"Materials/Railing.yaml" });
	railingCorner.name = "Railing Corner";
	railingCorner.serializedName = "RailingCorner";
	railingCorner.SetMeshIndex("edge");
	modelOptions.push_back(std::move(railingCorner));
}

EG_ON_INIT(OnInit)

MeshEnt::MeshEnt()
{
	m_scale = glm::vec3(1);
	
	m_physicsObject.canBePushed = false;
	m_physicsObject.debugColor = 0x12b81a;
}

void MeshEnt::RenderSettings()
{
	Ent::RenderSettings();
	ImGui::DragFloat3("Scale", &m_scale.x, 0.1f);
	
	if (ImGui::Button("Reset Rotation"))
	{
		m_rotation = glm::quat();
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
}

void MeshEnt::CommonDraw(const EntDrawArgs& args)
{
	if (!m_model.has_value())
		return;
	
	glm::mat4 transform =
		glm::translate(glm::mat4(1), m_position) *
		glm::mat4_cast(m_rotation) *
		glm::scale(glm::mat4(1), m_scale);
	
	if (modelOptions[*m_model].meshIndex != -1)
	{
		args.meshBatch->AddModelMesh(
			*modelOptions[*m_model].model,
			modelOptions[*m_model].meshIndex,
			*modelOptions[*m_model].materialOptions[m_material].second,
			StaticPropMaterial::InstanceData(transform)
		);
	}
	else
	{
		args.meshBatch->AddModel(
			*modelOptions[*m_model].model,
			*modelOptions[*m_model].materialOptions[m_material].second,
			StaticPropMaterial::InstanceData(transform)
		);
	}
}

void MeshEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
}

glm::quat MeshEnt::GetEditorRotation()
{
	return m_rotation;
}

void MeshEnt::EditorRotated(const glm::quat& newRotation)
{
	m_rotation = newRotation;
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
			break;
		}
	}
	
	if (m_model.has_value() && modelOptions[*m_model].collisionMesh.has_value())
	{
		m_physicsObject.position = m_position;
		m_physicsObject.rotation = m_rotation;
		m_physicsObject.rayIntersectMask = modelOptions[*m_model].blockGun ? RAY_MASK_BLOCK_GUN : 0;
		m_physicsObject.shape = &*modelOptions[*m_model].collisionMesh;
	}
}

void MeshEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	if (m_model.has_value() && modelOptions[*m_model].collisionMesh.has_value())
	{
		physicsEngine.RegisterObject(&m_physicsObject);
	}
}
