#pragma once

#include "LightSourceEntity.hpp"
#include "../../Graphics/Lighting/SpotLight.hpp"

class SpotLightEntity : public LightSourceEntity, public Entity::IEditorWallDrag
{
public:
	SpotLightEntity()
		: SpotLightEntity(eg::ColorSRGB(1, 1, 1), 5) { }
	
	SpotLightEntity(const eg::ColorSRGB& color, float intensity,
	                float cutoffAngle = eg::PI / 4, float penumbraAngle = eg::PI / 16);
	
	void InitDrawData(SpotLightDrawData& data) const;
	
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
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
	void EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal) override;
	
	void EditorRenderSettings() override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir) override;
	
	inline float GetWidth() const
	{
		return m_width;
	}
	
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
