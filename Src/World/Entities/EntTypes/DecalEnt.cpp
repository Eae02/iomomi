#include <imgui.h>
#include "DecalEnt.hpp"
#include "../../../Graphics/Materials/DecalMaterial.hpp"
#include "../../../../Protobuf/Build/DecalEntity.pb.h"

static const char* decalMaterials[] = 
{
	"Stain0",
	"Stain1",
	"Stain2",
	"Arrow"
};

DecalEnt::DecalEnt()
{
	UpdateMaterialPointer();
}

void DecalEnt::SetMaterialByName(std::string_view materialName)
{
	for (size_t i = 0; i < std::size(decalMaterials); i++)
	{
		if (decalMaterials[i] == materialName)
		{
			m_decalMaterialIndex = (int)i;
			UpdateMaterialPointer();
			return;
		}
	}
	
	eg::Log(eg::LogLevel::Error, "decal", "Decal material name not recognized: '{0}'", materialName);
}

const char* DecalEnt::GetMaterialName() const
{
	return decalMaterials[m_decalMaterialIndex];
}

void DecalEnt::UpdateMaterialPointer()
{
	std::string fullName = eg::Concat({ "Materials/Decals/", decalMaterials[m_decalMaterialIndex], ".yaml" });
	
	m_material = &eg::GetAsset<DecalMaterial>(fullName);
}

glm::mat4 DecalEnt::GetDecalTransform() const
{
	const float ar = m_material->AspectRatio();
	return glm::translate(glm::mat4(1), m_position) *
	       glm::mat4(GetRotationMatrix(m_direction)) *
	       glm::rotate(glm::mat4(1), rotation, glm::vec3(0, 1, 0)) *
	       glm::scale(glm::mat4(1), glm::vec3(scale * ar, 1.0f, scale));
}

void DecalEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	if (ImGui::Combo("Material", &m_decalMaterialIndex, decalMaterials, std::size(decalMaterials)))
	{
		UpdateMaterialPointer();
	}
	
	ImGui::DragFloat("Scale", &scale, 0.1f, 0.0f, INFINITY);
	ImGui::SliderAngle("Rotation", &rotation);
}

void DecalEnt::CommonDraw(const EntDrawArgs& args)
{
	args.meshBatch->Add(DecalMaterial::GetMesh(), *m_material, GetDecalTransform(), 1);
}

void DecalEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::DecalEntity decalPB;
	
	SerializePos(decalPB, m_position);
	
	decalPB.set_material_name(GetMaterialName());
	
	decalPB.set_dir((gravity_pb::Dir)m_direction);
	decalPB.set_rotation(rotation);
	decalPB.set_scale(scale);
	
	decalPB.SerializeToOstream(&stream);
}

void DecalEnt::Deserialize(std::istream& stream)
{
	gravity_pb::DecalEntity decalPB;
	decalPB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(decalPB);
	m_direction = (Dir)decalPB.dir();
	
	rotation = decalPB.rotation();
	scale = decalPB.scale();
	
	SetMaterialByName(decalPB.material_name());
}

void DecalEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	if (faceDirection)
		m_direction = *faceDirection;
}
