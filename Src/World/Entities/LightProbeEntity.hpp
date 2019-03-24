#pragma once

#include "Entity.hpp"

class LightProbeEntity : public Entity
{
public:
	LightProbeEntity();
	
	int GetEditorIconIndex() const override;
	
	void EditorRenderSettings() override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
	void GetParameters(struct LightProbe& probeOut) const;
	
private:
	glm::vec3 m_parallaxRadius;
	glm::vec3 m_influenceRadius;
	glm::vec3 m_influenceFade;
};
