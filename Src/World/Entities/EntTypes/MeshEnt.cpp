#include "MeshEnt.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"

struct MeshType
{
	const char* name;
	const eg::Model* model;
	const eg::IMaterial* material;
	
	MeshType(const char* _name, const char* modelName, const char* materialName)
		: name(_name)
	{
		std::string fullModelName = std::string("Models/") + modelName + ".obj";
		std::string fullMaterialName = std::string("Materials/") + modelName + ".yaml";
		model = &eg::GetAsset<eg::Model>(modelName);
		material = &eg::GetAsset<StaticPropMaterial>(materialName);
	}
};

std::vector<MeshType> meshTypes;

static void OnInit()
{
	//meshTypes.emplace_back("");
}

EG_ON_INIT(OnInit);

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
