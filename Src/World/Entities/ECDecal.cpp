#include "ECDecal.hpp"
#include "ECWallMounted.hpp"
#include "Messages.hpp"
#include "ECEditorVisible.hpp"
#include "../../Graphics/Materials/DecalMaterial.hpp"
#include "../../../Protobuf/Build/DecalEntity.pb.h"

#include <imgui.h>

eg::MessageReceiver ECDecal::MessageReceiver = eg::MessageReceiver::Create<ECDecal,
	DrawMessage, EditorDrawMessage, EditorRenderImGuiMessage>();

eg::EntitySignature ECDecal::Signature = eg::EntitySignature::Create<
	ECDecal, ECWallMounted, eg::ECPosition3D, ECEditorVisible>();

static const char* decalMaterials[] = 
{
	"Stain0"
};

ECDecal::ECDecal()
{
	UpdateMaterialPointer();
}

void ECDecal::SetMaterialByName(std::string_view materialName)
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

const char* ECDecal::GetMaterialName() const
{
	return decalMaterials[m_decalMaterialIndex];
}

void ECDecal::UpdateMaterialPointer()
{
	std::string fullName = eg::Concat({ "Materials/Decals/", decalMaterials[m_decalMaterialIndex], ".yaml" });
	
	m_material = &eg::GetAsset<DecalMaterial>(fullName);
}

glm::mat4 ECDecal::GetTransform(const eg::Entity& entity)
{
	const ECDecal& decal = entity.GetComponent<ECDecal>();
	
	float ar = decal.m_material->AspectRatio();
	
	return ECWallMounted::GetTransform(entity, 1.0f) *
	       glm::rotate(glm::mat4(1), decal.rotation, glm::vec3(0, 1, 0)) *
	       glm::scale(glm::mat4(1), glm::vec3(decal.scale * ar, 1.0f, decal.scale));
}

void ECDecal::HandleMessage(eg::Entity& entity, const DrawMessage& message)
{
	message.meshBatch->Add(DecalMaterial::GetMesh(), *m_material, GetTransform(entity), 1);
}

void ECDecal::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
{
	message.meshBatch->Add(DecalMaterial::GetMesh(), *m_material, GetTransform(entity), 1);
}

void ECDecal::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
{
	ECEditorVisible::RenderDefaultSettings(entity);
	
	if (ImGui::Combo("Material", &m_decalMaterialIndex, decalMaterials, std::size(decalMaterials)))
	{
		UpdateMaterialPointer();
	}
	
	ImGui::DragFloat("Scale", &scale, 0.1f, 0.0f, INFINITY);
	ImGui::SliderAngle("Rotation", &rotation);
}

eg::Entity* ECDecal::CreateEntity(eg::EntityManager& entityManager)
{
	eg::Entity& entity = entityManager.AddEntity(Signature, nullptr, EntitySerializer);
	
	entity.InitComponent<ECEditorVisible>("Decal");
	
	return &entity;
}

struct DecalSerializer : eg::IEntitySerializer
{
	std::string_view GetName() const override
	{
		return "Decal";
	}
	
	void Serialize(const eg::Entity& entity, std::ostream& stream) const override
	{
		gravity_pb::DecalEntity decalPB;
		
		glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
		decalPB.set_posx(pos.x);
		decalPB.set_posy(pos.y);
		decalPB.set_posz(pos.z);
		
		const ECDecal& decal = entity.GetComponent<ECDecal>();
		
		decalPB.set_materialname(decal.GetMaterialName());
		
		decalPB.set_dir((gravity_pb::Dir)entity.GetComponent<ECWallMounted>().wallUp);
		
		decalPB.set_rotation(decal.rotation);
		decalPB.set_scale(decal.scale);
		
		decalPB.SerializeToOstream(&stream);
	}
	
	void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
	{
		eg::Entity& entity = *ECDecal::CreateEntity(entityManager);
		
		gravity_pb::DecalEntity decalPB;
		decalPB.ParseFromIstream(&stream);
		
		entity.GetComponent<ECWallMounted>().wallUp = (Dir)decalPB.dir();
		
		ECDecal& decal = entity.GetComponent<ECDecal>();
		
		decal.rotation = decalPB.rotation();
		decal.scale = decalPB.scale();
		
		decal.SetMaterialByName(decalPB.materialname());
		
		entity.InitComponent<eg::ECPosition3D>(decalPB.posx(), decalPB.posy(), decalPB.posz());
	}
};

eg::IEntitySerializer* ECDecal::EntitySerializer = new DecalSerializer;
