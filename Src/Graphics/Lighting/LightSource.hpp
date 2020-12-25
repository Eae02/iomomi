#pragma once

class LightSource
{
public:
	LightSource(const eg::ColorSRGB& color, float intensity);
	
	void SetRadiance(const eg::ColorSRGB& color, float intensity);
	
	inline float Range() const
	{
		return m_range;
	}
	
	const glm::vec3& Radiance() const
	{
		return m_radiance;
	}
	
	const eg::ColorSRGB& GetColor() const
	{
		return m_color;
	}
	
	float Intensity() const
	{
		return m_intensity;
	}
	
	uint64_t InstanceID() const
	{
		return m_instanceID;
	}
	
	static uint64_t NextInstanceID();
	
private:
	float m_range;
	uint64_t m_instanceID;
	
	glm::vec3 m_radiance;
	eg::ColorSRGB m_color;
	float m_intensity;
};
