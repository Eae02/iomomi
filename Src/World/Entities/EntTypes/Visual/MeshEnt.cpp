#include "MeshEnt.hpp"
#include "../../../../Graphics/Materials/StaticPropMaterial.hpp"

struct MeshType
{
	std::string_view name;
	const eg::Model* model;
	std::vector<const eg::IMaterial*> materials;
	
	MeshType(std::string_view _name, std::string_view modelName)
		: name(_name), model(&eg::GetAsset<eg::Model>(modelName))
	{
		materials.resize(model->NumMaterials(), &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml"));
	}
};

std::vector<MeshType> meshTypes;

static void OnInit()
{
	
}

EG_ON_INIT(OnInit)

MeshEnt::MeshEnt()
{
	
}

void MeshEnt::Serialize(std::ostream& stream) const
{
	
}

void MeshEnt::Deserialize(std::istream& stream)
{
	
}

void MeshEnt::RenderSettings()
{
	Ent::RenderSettings();
}

void MeshEnt::CommonDraw(const EntDrawArgs& args)
{
	
}

const void* MeshEnt::GetComponent(const std::type_info& type) const
{
	return Ent::GetComponent(type);
}

void MeshEnt::EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
}
