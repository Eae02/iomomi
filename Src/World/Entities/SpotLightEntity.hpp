#pragma once

#include "Entity.hpp"
#include "../../Graphics/Lighting/SpotLight.hpp"

class SpotLightEntity : public Entity
{
public:
	SpotLightEntity()
		: SpotLightEntity(eg::ColorSRGB(1, 1, 1), 5, 0, 0) { }
	
	SpotLightEntity(const eg::ColorSRGB& color, float intensity, float yaw, float pitch,
	                float cutoffAngle = eg::PI / 4, float penumbraAngle = eg::PI / 16);
	
	void InitDrawData(SpotLightDrawData& data) const;
	
	void SetRadiance(const eg::ColorSRGB& color, float intensity);
	
	inline const glm::vec3& GetDirection() const
	{
		return m_rotationMatrix[1];
	}
	
	inline const glm::vec3& GetDirectionL() const
	{
		return m_rotationMatrix[0];
	}
	
	void SetDirection(float yaw, float pitch);
	
	void SetCutoff(float cutoffAngle, float penumbraAngle);
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
	void EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal) override;
	
	bool EditorInteract(const EditorInteractArgs& args) override;
	
	void EditorRenderSettings() override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
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
	
	float m_yaw;
	float m_pitch;
	
	float m_cutoffAngle;
	float m_penumbraAngle;
	
	float m_penumbraBias;
	float m_penumbraScale;
	
	uint64_t m_instanceID;
	
	eg::ColorSRGB m_color;
	float m_intensity;
	
	glm::mat3 m_rotationMatrix;
	glm::vec3 m_radiance;
};
