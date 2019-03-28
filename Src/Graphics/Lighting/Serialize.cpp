#include "Serialize.hpp"
#include "../../YAMLUtils.hpp"

inline void SerializeLightSource(YAML::Emitter& emitter, const LightSource& light)
{
	const eg::ColorSRGB& color = light.GetColor();
	WriteYAMLVec3(emitter, "color", glm::vec3(color.r, color.g, color.b));
	
	emitter << YAML::Key << "intensity" << YAML::Value << light.Intensity();
}

void DeserializeLightSource(const YAML::Node& node, LightSource& light)
{
	glm::vec3 colorV3 = ReadYAMLVec3(node["color"]);
	light.SetRadiance(eg::ColorSRGB(colorV3.r, colorV3.g, colorV3.b), node["intensity"].as<float>(20.0f));
}

void SerializePointLight(YAML::Emitter& emitter, const PointLight& pointLight)
{
	SerializeLightSource(emitter, pointLight);
}

void DeserializePointLight(const YAML::Node& node, PointLight& pointLight)
{
	DeserializeLightSource(node, pointLight);
}

void SerializeSpotLight(YAML::Emitter& emitter, const SpotLight& spotLight)
{
	SerializeLightSource(emitter, spotLight);
	
	emitter << YAML::Key << "cutoff" << YAML::Value << spotLight.CutoffAngle()
	        << YAML::Key << "penumbra" << YAML::Value << spotLight.PenumbraAngle();
	
	WriteYAMLVec3(emitter, "dir", spotLight.Direction());
}

void DeserializeSpotLight(const YAML::Node& node, SpotLight& spotLight)
{
	DeserializeLightSource(node, spotLight);
	
	spotLight.SetCutoff(node["cutoff"].as<float>(eg::PI / 4), node["penumbra"].as<float>(eg::PI / 16));
	
	if (const YAML::Node& dirNode = node["dir"])
	{
		spotLight.SetDirection(ReadYAMLVec3(dirNode));
	}
}
