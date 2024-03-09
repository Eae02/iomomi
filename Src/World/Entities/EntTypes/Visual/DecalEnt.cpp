#include "DecalEnt.hpp"

#include "../../../../../Protobuf/Build/DecalEntity.pb.h"
#include "../../../../Graphics/Materials/DecalMaterial.hpp"
#include "../../../../ImGui.hpp"

DEF_ENT_TYPE(DecalEnt)

static const char* decalMaterials[] = { "Stain0", "Stain1", "Stain2", "Arrow", "WarningDecal", "Target" };

DecalEnt::DecalEnt() : m_repetitions(0, 0)
{
	UpdateMaterialPointer();
}

void DecalEnt::SetMaterialByName(std::string_view materialName)
{
	for (size_t i = 0; i < std::size(decalMaterials); i++)
	{
		if (decalMaterials[i] == materialName)
		{
			m_decalMaterialIndex = static_cast<int>(i);
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
	std::string fullName = eg::Concat({ "Decals/", decalMaterials[m_decalMaterialIndex], ".yaml" });

	m_material = &eg::GetAsset<DecalMaterial>(fullName);
}

glm::mat4 DecalEnt::GetDecalTransform() const
{
	const float ar = m_material->AspectRatio();
	return glm::translate(glm::mat4(1), m_position) * glm::mat4(GetRotationMatrix(m_direction)) *
	       glm::rotate(glm::mat4(1), rotation, glm::vec3(0, 1, 0)) *
	       glm::scale(glm::mat4(1), glm::vec3(scale * ar * (m_repetitions.x + 1), 1.0f, scale * (m_repetitions.y + 1)));
}

void DecalEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	Ent::RenderSettings();

	if (ImGui::Combo("Material", &m_decalMaterialIndex, decalMaterials, std::size(decalMaterials)))
	{
		UpdateMaterialPointer();
	}

	ImGui::DragFloat("Scale", &scale, 0.1f, 0.0f, INFINITY);
	ImGui::SliderAngle("Rotation", &rotation);

	ImGui::DragInt2("Repetitions", &m_repetitions.x);
#endif
}

void DecalEnt::CommonDraw(const EntDrawArgs& args)
{
	const glm::mat4 transform = GetDecalTransform();
	const eg::AABB aabb = eg::AABB(glm::vec3(-1, 0, -1), glm::vec3(1, 0.01f, 1)).TransformedBoundingBox(transform);
	if (args.frustum->Intersects(aabb))
	{
		args.meshBatch->Add(
			DecalMaterial::GetMesh(), *m_material,
			DecalMaterial::InstanceData(transform, glm::vec2(0, 0), glm::vec2(m_repetitions + 1)), 1
		);
	}
}

void DecalEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	CommonDraw(args);
	if (args.getDrawMode(this) == EntEditorDrawMode::Selected)
	{
		const glm::mat4 transform = GetDecalTransform();
		const glm::vec3 vbl = transform * glm::vec4(-1, 0, -1, 1);
		const glm::vec3 vbr = transform * glm::vec4(1, 0, -1, 1);
		const glm::vec3 vtl = transform * glm::vec4(-1, 0, 1, 1);
		const glm::vec3 vtr = transform * glm::vec4(1, 0, 1, 1);
		static const eg::ColorSRGB editorOutlineColor = eg::ColorSRGB::FromHex(0x1f9bde);

		args.primitiveRenderer->AddLine(vbl, vbr, editorOutlineColor, 0.02f);
		args.primitiveRenderer->AddLine(vbr, vtr, editorOutlineColor, 0.02f);
		args.primitiveRenderer->AddLine(vtr, vtl, editorOutlineColor, 0.02f);
		args.primitiveRenderer->AddLine(vtl, vbl, editorOutlineColor, 0.02f);
	}
}

void DecalEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::DecalEntity decalPB;

	SerializePos(decalPB, m_position);

	decalPB.set_material_name(GetMaterialName());

	decalPB.set_dir(static_cast<iomomi_pb::Dir>(m_direction));
	decalPB.set_rotation(rotation);
	decalPB.set_scale(scale);
	decalPB.set_extension_x(m_repetitions.x);
	decalPB.set_extension_y(m_repetitions.y);

	decalPB.SerializeToOstream(&stream);
}

void DecalEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::DecalEntity decalPB;
	decalPB.ParseFromIstream(&stream);

	m_position = DeserializePos(decalPB);
	m_direction = static_cast<Dir>(decalPB.dir());
	m_repetitions.x = decalPB.extension_x();
	m_repetitions.y = decalPB.extension_y();

	rotation = decalPB.rotation();
	scale = decalPB.scale();

	SetMaterialByName(decalPB.material_name());
}

void DecalEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	if (faceDirection)
		m_direction = *faceDirection;
}

float DecalEnt::GetWallRotation() const
{
	return rotation;
}

void DecalEnt::SetWallRotation(float _rotation)
{
	rotation = _rotation;
}

int DecalEnt::EdGetIconIndex() const
{
	return 19;
}
