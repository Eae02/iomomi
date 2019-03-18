#include "LightSourceEntity.hpp"

#include <imgui.h>

static std::atomic<uint64_t> nextInstanceID(0);

LightSourceEntity::LightSourceEntity(const eg::ColorSRGB& color, float intensity)
	: m_instanceID(nextInstanceID++)
{
	SetRadiance(color, intensity);
}

void LightSourceEntity::SetRadiance(const eg::ColorSRGB& color, float intensity)
{
	m_radiance.r = eg::SRGBToLinear(color.r) * intensity;
	m_radiance.g = eg::SRGBToLinear(color.g) * intensity;
	m_radiance.b = eg::SRGBToLinear(color.b) * intensity;
	
	const float maxChannel = std::max(m_radiance.r, std::max(m_radiance.g, m_radiance.b));
	m_range = std::sqrt(256 * maxChannel + 1.0f);
	
	m_color = color;
	m_intensity = intensity;
}

void LightSourceEntity::EditorRenderSettings()
{
	Entity::EditorRenderSettings();
	
	if (ImGui::ColorEdit3("Color", &m_color.r))
	{
		SetRadiance(m_color, m_intensity);
	}
	
	if (ImGui::DragFloat("Intensity", &m_intensity, 0.1f, 0.0f, INFINITY, "%.1f"))
	{
		SetRadiance(m_color, m_intensity);
	}
}

void LightSourceEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
	
	emitter << YAML::Key << "range" << YAML::Value << m_range
	        << YAML::Key << "intensity" << YAML::Value << m_intensity
	        << YAML::Key << "color" << YAML::Value << YAML::BeginSeq
	        << m_color.r << m_color.g << m_color.b << YAML::EndSeq;
}

void LightSourceEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
	
	eg::ColorSRGB color(1, 1, 1);
	if (const YAML::Node& colorNode = node["color"])
	{
		color.r = colorNode[0].as<float>(1);
		color.g = colorNode[1].as<float>(1);
		color.b = colorNode[2].as<float>(1);
	}
	
	SetRadiance(color, node["intensity"].as<float>(1));
}
