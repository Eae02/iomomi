#pragma once

#include "../EditorComponent.hpp"

class EntityEditorComponent : public EditorComponent
{
public:
	bool UpdateInput(float dt, const EditorState& editorState) override;
	
	void RenderSettings(const EditorState& editorState) override;
	
	void LateDraw(const EditorState& editorState) const override;
	
	bool CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons) override;
	
private:
	std::optional<glm::ivec2> m_mouseDownPos;
	
	struct DraggingEntity
	{
		std::weak_ptr<Ent> entity;
		glm::vec3 initialPosition;
		glm::vec3 lastPositionSet;
		
		DraggingEntity(std::weak_ptr<Ent> _entity, const glm::vec3& position)
			: entity(std::move(_entity)), initialPosition(position), lastPositionSet(position) { }
	};
	std::vector<DraggingEntity> m_dragEntities;
	
	glm::vec3 m_gizmoPos;
	glm::vec3 m_initialGizmoPos;
	glm::vec3 m_gizmoDragDist;
	eg::TranslationGizmo m_translationGizmo;
	bool m_drawTranslationGizmo = false;
	bool m_entitiesCloned = false;
	
	bool m_isDraggingWallEntity = false;
	
	uint64_t m_lastUpdateFrameIndex = 0;
};
