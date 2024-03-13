#include <fstream>
#include <magic_enum/magic_enum.hpp>
#include <yaml-cpp/yaml.h>

#include "../StaticPropMaterial.hpp"

static const eg::AssetFormat StaticPropMaterialAssetFormat{ "StaticPropMaterial", 10 };

#ifdef EG_HAS_YAML_CPP
class StaticPropMaterialAssetGenerator : public eg::AssetGenerator
{
public:
	bool Generate(eg::AssetGenerateContext& generateContext) override
	{
		std::string relSourcePath = generateContext.RelSourcePath();
		std::string sourcePath = generateContext.FileDependency(relSourcePath);
		std::ifstream stream(sourcePath, std::ios::binary);
		if (!stream)
			return false;

		YAML::Node rootYaml = YAML::Load(stream);

		const float roughnessMin = rootYaml["roughnessMin"].as<float>(0.0f);
		const float roughnessMax = rootYaml["roughnessMax"].as<float>(1.0f);
		const float texScale = rootYaml["textureScale"].as<float>(1.0f);
		const float texScaleX = rootYaml["textureScaleX"].as<float>(texScale);
		const float texScaleY = rootYaml["textureScaleY"].as<float>(texScale);
		const bool backfaceCull = rootYaml["backfaceCull"].as<bool>(true);
		const bool backfaceCullEd = rootYaml["backfaceCullEd"].as<bool>(false);
		const bool castShadows = rootYaml["castShadows"].as<bool>(true);
		const bool alphaTest = rootYaml["alphaTest"].as<bool>(false);

		QualityLevel minShadowQuality = QualityLevel::VeryLow;
		std::string minShadowQualityStr = rootYaml["minShadowQuality"].as<std::string>("");
		if (!minShadowQualityStr.empty())
		{
			if (std::optional<QualityLevel> levelOpt = magic_enum::enum_cast<QualityLevel>(minShadowQualityStr))
			{
				minShadowQuality = *levelOpt;
			}
			else
			{
				eg::Log(
					eg::LogLevel::Error, "as", "Unknown shadow quality level {} in {}", minShadowQualityStr, sourcePath
				);
			}
		}

		std::string albedoPath = rootYaml["albedo"].as<std::string>(std::string());
		std::string normalMapPath = rootYaml["normalMap"].as<std::string>(std::string());
		std::string miscMapPath = rootYaml["miscMap"].as<std::string>(std::string());

		if (albedoPath.empty() || normalMapPath.empty() || miscMapPath.empty())
		{
			eg::Log(
				eg::LogLevel::Error, "as",
				"Invalid static prop material '{0}', missing one of 'albedo', 'normalMap', or 'miscMap'", relSourcePath
			);
			return false;
		}

		generateContext.writer.WriteString(albedoPath);
		generateContext.writer.WriteString(normalMapPath);
		generateContext.writer.WriteString(miscMapPath);
		generateContext.writer.Write(roughnessMin);
		generateContext.writer.Write(roughnessMax);
		generateContext.writer.Write(texScaleX);
		generateContext.writer.Write(texScaleY);
		generateContext.writer.Write<uint8_t>(
			static_cast<uint8_t>(backfaceCull) | (static_cast<uint8_t>(backfaceCullEd) << 1)
		);
		generateContext.writer.Write<uint8_t>(static_cast<uint8_t>(castShadows));
		generateContext.writer.Write<uint8_t>(static_cast<uint8_t>(minShadowQuality));
		generateContext.writer.Write<uint8_t>(static_cast<uint8_t>(alphaTest));

		generateContext.AddLoadDependency(std::move(albedoPath));
		generateContext.AddLoadDependency(std::move(normalMapPath));
		generateContext.AddLoadDependency(std::move(miscMapPath));

		generateContext.AddLoadDependency("/Shaders/Common3D.vs.glsl");
		generateContext.AddLoadDependency("/Shaders/StaticModel.fs.glsl");
		generateContext.AddLoadDependency("/Shaders/PointLightShadow.fs.glsl");
		generateContext.AddLoadDependency("/Shaders/Common3D-PLShadow.vs.glsl");

		return true;
	}
};
#endif

bool StaticPropMaterialAssetLoader(const eg::AssetLoadContext& loadContext)
{
	StaticPropMaterial::LazyInitGlobals();

	eg::MemoryStreambuf memoryStreambuf(loadContext.Data());
	std::istream stream(&memoryStreambuf);

	std::string albedoTextureName = eg::BinReadString(stream);
	std::string normalMapTextureName = eg::BinReadString(stream);
	std::string miscMapTextureName = eg::BinReadString(stream);

	std::string albedoTexturePath = eg::Concat({ loadContext.DirPath(), albedoTextureName });
	std::string normalMapTexturePath = eg::Concat({ loadContext.DirPath(), normalMapTextureName });
	std::string miscMapTexturePath = eg::Concat({ loadContext.DirPath(), miscMapTextureName });

	StaticPropMaterial& material = loadContext.CreateResult<StaticPropMaterial>();
	material.m_albedoTexture = &eg::GetAsset<eg::Texture>(albedoTexturePath);
	material.m_normalMapTexture = &eg::GetAsset<eg::Texture>(normalMapTexturePath);
	material.m_miscMapTexture = &eg::GetAsset<eg::Texture>(miscMapTexturePath);

	material.m_roughnessMin = eg::BinRead<float>(stream);
	material.m_roughnessMax = eg::BinRead<float>(stream);
	material.m_textureScale.x = 1.0f / eg::BinRead<float>(stream);
	material.m_textureScale.y = 1.0f / eg::BinRead<float>(stream);
	uint8_t backfaceCullFlags = eg::BinRead<uint8_t>(stream);
	material.m_backfaceCull = (backfaceCullFlags & 1) != 0;
	material.m_backfaceCullEditor = (backfaceCullFlags & 2) != 0;
	material.m_castShadows = eg::BinRead<uint8_t>(stream);
	material.m_minShadowQuality = (QualityLevel)eg::BinRead<uint8_t>(stream);
	material.m_alphaTest = eg::BinRead<uint8_t>(stream);

	material.CreateDescriptorSetAndParamsBuffer();

	return true;
}

void InitializeStaticPropMaterialAsset()
{
#ifdef EG_HAS_YAML_CPP
	eg::RegisterAssetGenerator<StaticPropMaterialAssetGenerator>("StaticPropMaterial", StaticPropMaterialAssetFormat);
#endif
	eg::RegisterAssetLoader("StaticPropMaterial", &StaticPropMaterialAssetLoader, StaticPropMaterialAssetFormat);
}
