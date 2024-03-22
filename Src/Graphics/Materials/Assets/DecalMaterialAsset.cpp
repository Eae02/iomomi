#include <fstream>

#include "../DecalMaterial.hpp"
#include <yaml-cpp/yaml.h>

static const eg::AssetFormat DecalMaterialAssetFormat{ "DecalMaterial", 3 };

#ifdef EG_HAS_YAML_CPP
class DecalMaterialAssetGenerator : public eg::AssetGenerator
{
public:
	bool Generate(eg::AssetGenerateContext& generateContext)
	{
		std::string relSourcePath = generateContext.RelSourcePath();
		std::string sourcePath = generateContext.FileDependency(relSourcePath);
		std::ifstream stream(sourcePath, std::ios::binary);
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

		generateContext.writer.WriteString(albedoPath);
		generateContext.writer.WriteString(normalMapPath);
		generateContext.writer.Write(roughness);
		generateContext.writer.Write(opacity);
		generateContext.writer.Write<uint8_t>(inheritNormals);

		generateContext.AddLoadDependency(std::move(albedoPath));
		generateContext.AddLoadDependency(std::move(normalMapPath));

		generateContext.AddLoadDependency("/Shaders/Decal.vs.glsl");
		generateContext.AddLoadDependency("/Shaders/Decal.fs.glsl");

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

	std::string albedoTexturePath = eg::Concat({ loadContext.DirPath(), albedoTextureName });
	std::string normalMapTexturePath = eg::Concat({ loadContext.DirPath(), normalMapTextureName });

	DecalMaterial::LazyInitGlobals();

	DecalMaterial::Parameters parameters;
	parameters.roughness = eg::BinRead<float>(stream);
	parameters.opacity = eg::BinRead<float>(stream);
	parameters.inheritNormals = eg::BinRead<uint8_t>(stream) != 0;

	loadContext.CreateResult<DecalMaterial>(
		eg::GetAsset<eg::Texture>(albedoTexturePath), eg::GetAsset<eg::Texture>(normalMapTexturePath), parameters
	);

	return true;
}

void InitializeDecalMaterialAsset()
{
#ifdef EG_HAS_YAML_CPP
	eg::RegisterAssetGenerator<DecalMaterialAssetGenerator>("DecalMaterial", DecalMaterialAssetFormat);
#endif
	eg::RegisterAssetLoader("DecalMaterial", &DecalMaterialAssetLoader, DecalMaterialAssetFormat);
}
