#pragma once

#include "Entity.hpp"
#include "../Graphics/Lighting/SpotLight.hpp"

class SpotLightEntity : public Entity
{
public:
	inline SpotLightEntity()
		: SpotLightEntity(glm::vec3(0.0f), eg::ColorLin(1, 1, 1), glm::vec3(0, 0, 1)) { }
	
	SpotLightEntity(const glm::vec3& position, const eg::ColorLin& radiance, const glm::vec3& direction,
	                float cutoffAngle = eg::PI / 4, float penumbraAngle = eg::PI / 16);
	
	void InitDrawData(SpotLightDrawData& data) const;
	
	void SetRadiance(const eg::ColorLin& radiance);
	
	inline const glm::vec3& GetDirection() const
	{
		return m_rotationMatrix[1];
	}
	
	inline const glm::vec3& GetDirectionL() const
	{
		return m_rotationMatrix[0];
	}
	
	void SetDirection(const glm::vec3& direction);
	
	void SetCutoff(float cutoffAngle, float penumbraAngle);
	
	bool EditorInteract(const EditorInteractArgs& args) override;
	
	inline float GetRange() const
	{
		return m_range;
	}
	
	inline float GetWidth() const
	{
		return m_width;
	}
	
	inline uint64_t GetInstanceID() const
	{
		return m_instanceID;
	}
	
private:
	float m_range = 0.0f;
	float m_width = 0.0f;
	
	float m_penumbraBias;
	float m_penumbraScale;
	
	uint64_t m_instanceID;
	
	glm::mat3 m_rotationMatrix;
	glm::vec3 m_radiance;
};
