#include "../DecalMaterial.hpp"

#include <fstream>

static const eg::AssetFormat DecalMaterialAssetFormat { "DecalMaterial", 2 };

#ifdef EG_HAS_YAML_CPP
class DecalMaterialAssetGenerator : public eg::AssetGenerator
{
public:
	bool Generate(eg::AssetGenerateContext& generateContext)
	{
		std::string relSourcePath = generateContext.RelSourcePath();
		std::string sourcePath = generateContext.FileDependency(relSourcePath);
		std::ifstream stream(sourcePath);
		EG_ASSERT(stream)
		
		YAML::Node rootYaml = YAML::Load(stream);
		
		const float roughness = rootYaml["roughness"].as<float>(1.0f);
		const float opacity = rootYaml["opacity"].as<float>(1.0f);
		const bool inheritNormals = rootYaml["inheritNormals"].as<bool>(false);
		
		std::string albedoPath = rootYaml["albedo"].as<std::string>(std::string());
		std::string normalMapPath = rootYaml["normalMap"].as<std::string>(std::string());
		
		if (albedoPath.empty())
		{
			eg::Log(eg::LogLevel::Error, "as", "Invalid decal material '{0}', missing 'albedo' entry", relSourcePath);
			return false;
		}
		
		eg::BinWriteString(generateContext.outputStream, albedoPath);
		eg::BinWriteString(generateContext.outputStream, normalMapPath);
		eg::BinWrite(generateContext.outputStream, roughness);
		eg::BinWrite(generateContext.outputStream, opacity);
		eg::BinWrite<uint8_t>(generateContext.outputStream, inheritNormals);
		
		generateContext.AddLoadDependency(std::move(albedoPath));
		generateContext.AddLoadDependency(std::move(normalMapPath));
		
		return true;
	}
};
#endif

bool DecalMaterialAssetLoader(const eg::AssetLoadContext& loadContext)
{
	eg::MemoryStreambuf memoryStreambuf(loadContext.Data());
	std::istream stream(&memoryStreambuf);
	
	std::string albedoTextureName = eg::BinReadString(stream);
	std::string normalMapTextureName = eg::BinReadString(stream);
	
	std::string albedoTexturePath = eg::Concat({ loadContext.DirPath(), albedoTextureName} );
	std::string normalMapTexturePath = eg::Concat({ loadContext.DirPath(), normalMapTextureName} );
	
	DecalMaterial& material = loadContext.CreateResult<DecalMaterial>(
		eg::GetAsset<eg::Texture>(albedoTexturePath),
		eg::GetAsset<eg::Texture>(normalMapTexturePath)
	);
	
	material.m_roughness = eg::BinRead<float>(stream);
	material.m_opacity = eg::BinRead<float>(stream);
	material.m_inheritNormals = eg::BinRead<uint8_t>(stream);
	
	return true;
}

void InitializeDecalMaterialAsset()
{
#ifdef EG_HAS_YAML_CPP
	eg::RegisterAssetGenerator<DecalMaterialAssetGenerator>("DecalMaterial", DecalMaterialAssetFormat);
#endif
	eg::RegisterAssetLoader("DecalMaterial", &DecalMaterialAssetLoader, DecalMaterialAssetFormat);
}
