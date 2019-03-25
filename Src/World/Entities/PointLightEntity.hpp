#pragma once

#include "LightSourceEntity.hpp"
#include "../../Graphics/Lighting/PointLight.hpp"

class PointLightEntity : public LightSourceEntity
{
public:
	PointLightEntity()
		: PointLightEntity(eg::ColorSRGB(1, 1, 1), 10) { }
	
	PointLightEntity(const eg::ColorSRGB& color, float intensity)
		: LightSourceEntity(color, intensity) { }
	
	virtual void InitDrawData(PointLightDrawData& data) const;
	
	void EditorRenderSettings() override;
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
	int GetEditorIconIndex() const override;
};
