#pragma once

#include "../EditorComponent.hpp"
#include "../../World/Entities/EntTypes/ActivationLightStripEnt.hpp"

class LightStripEditorComponent : public EditorComponent
{
public:
	bool UpdateInput(float dt, const EditorState& editorState) override;
	
	bool CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons) override;
	
private:
	std::weak_ptr<Ent> m_connectingActivator;
	
	glm::vec3 m_lightStripInsertPos;
	Dir m_lightStripInsertNormal;
	
	std::weak_ptr<ActivationLightStripEnt> m_editingLightStripEntity;
	int m_editingWayPointIndex = -1;
};
