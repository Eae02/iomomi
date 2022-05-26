#include "LightSource.hpp"

static std::atomic<uint64_t> nextInstanceID(0);

LightSource::LightSource(const eg::ColorSRGB& color, float intensity)
	: m_instanceID(nextInstanceID++)
{
	SetRadiance(color, intensity);
}

void LightSource::SetRadiance(const eg::ColorSRGB& color, float intensity)
{
	m_radiance.r = eg::SRGBToLinear(color.r) * intensity;
	m_radiance.g = eg::SRGBToLinear(color.g) * intensity;
	m_radiance.b = eg::SRGBToLinear(color.b) * intensity;
	
	const float maxChannel = std::max(m_radiance.r, std::max(m_radiance.g, m_radiance.b));
	m_range = std::min(maxRange, std::sqrt(256 * maxChannel + 1.0f));
	
	m_color = color;
	m_intensity = intensity;
}

uint64_t LightSource::NextInstanceID()
{
	return nextInstanceID++;
}
