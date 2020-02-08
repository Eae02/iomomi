#pragma once

#include "../EditorComponent.hpp"

class EntityEditorComponent : public EditorComponent
{
public:
	bool UpdateInput(float dt, const EditorState& editorState) override;
	
	void RenderSettings(const EditorState& editorState) override;
	
	void LateDraw() const override;
	
	bool CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons) override;
	
private:
	glm::vec3 m_gizmoPosUnaligned;
	glm::vec3 m_prevGizmoPos;
	eg::TranslationGizmo m_translationGizmo;
	bool m_drawTranslationGizmo = false;
	bool m_entitiesCloned = false;
	
	glm::vec2 m_mouseDownPos;
	
	bool m_isDraggingWallEntity = false;
};
