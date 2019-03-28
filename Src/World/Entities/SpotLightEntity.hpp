#pragma once

#include "Entity.hpp"
#include "ISpotLightEntity.hpp"

class SpotLightEntity : public Entity, public ISpotLightEntity, public Entity::IEditorWallDrag
{
public:
	SpotLightEntity()
		: SpotLightEntity(eg::ColorSRGB(1, 1, 1), 5) { }
	
	SpotLightEntity(const eg::ColorSRGB& color, float intensity,
	                float cutoffAngle = eg::PI / 4, float penumbraAngle = eg::PI / 16)
		: m_spotLight(color, intensity, cutoffAngle, penumbraAngle) { }
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
	void EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal) override;
	
	void EditorRenderSettings() override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir) override;
	
	void GetSpotLights(std::vector<SpotLightDrawData>& drawData) const override;
	
private:
	SpotLight m_spotLight;
};
