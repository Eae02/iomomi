#pragma once

#include "LightSource.hpp"

struct SpotLightDrawData
{
#pragma pack(push, 1)
	struct
	{
		glm::vec3 position;
		float range;
		glm::vec3 direction;
		float penumbraBias;
		glm::vec3 directionL;
		float penumbraScale;
		glm::vec3 radiance;
		float width;
	} pc;
#pragma pack(pop)
	eg::TextureRef shadowMap;
	uint64_t instanceID;
	bool castsShadows;
};

class SpotLight : public LightSource
{
public:
	SpotLight()
		: SpotLight(eg::ColorSRGB(1, 1, 1), 5) { }
	
	SpotLight(const eg::ColorSRGB& color, float intensity, float cutoffAngle = eg::PI / 4,
		float penumbraAngle = eg::PI / 16);
	
	SpotLightDrawData GetDrawData(const glm::vec3& position) const;
	
	void SetDirection(const glm::vec3& direction);
	
	void SetCutoff(float cutoffAngle, float penumbraAngle);
	
	void DrawCone(class PrimitiveRenderer& renderer, const glm::vec3& position) const;
	
	float CutoffAngle() const
	{
		return m_cutoffAngle;
	}
	
	float PenumbraAngle() const
	{
		return m_penumbraAngle;
	}
	
	const glm::vec3& Direction() const
	{
		return m_rotationMatrix[1];
	}
	
	const glm::vec3& DirectionL() const
	{
		return m_rotationMatrix[0];
	}
	
	float GetWidth() const
	{
		return m_width;
	}
	
	bool castsShadows = true;
	
private:
	float m_width = 0.0f;
	
	float m_yaw;
	float m_pitch;
	
	float m_cutoffAngle;
	float m_penumbraAngle;
	
	float m_penumbraBias;
	float m_penumbraScale;
	
	glm::mat3 m_rotationMatrix;
};
