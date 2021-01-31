#pragma once

#include "../EditorComponent.hpp"

#include <EGame/RotationGizmo.hpp>

class EntityEditorComponent : public EditorComponent
{
public:
	EntityEditorComponent();
	
	bool UpdateInput(float dt, const EditorState& editorState) override;
	
	void RenderSettings(const EditorState& editorState) override;
	
	void LateDraw(const EditorState& editorState) const override;
	
	bool CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons) override;
	
private:
	std::optional<glm::ivec2> m_mouseDownPos;
	
	struct TransformingEntity
	{
		std::weak_ptr<Ent> entity;
		glm::vec3 initialPosition;
		glm::vec3 lastPositionSet;
		glm::quat initialRotation;
	};
	std::vector<TransformingEntity> m_transformingEntities;
	
	glm::vec3 m_gizmoPos;
	glm::vec3 m_initialGizmoPos;
	glm::vec3 m_gizmoDragDist;
	glm::quat m_rotationValue;
	
	eg::TranslationGizmo m_translationGizmo;
	eg::RotationGizmo m_rotationGizmo;
	bool m_drawTranslationGizmo = false;
	bool m_entitiesCloned = false;
	bool m_drawRotationGizmo = false;
	bool m_inRotationMode = false;
	bool m_canRotate = false;
	
	std::shared_ptr<Ent> m_wallDragEntity;
	bool m_isDraggingWallEntity = false;
	
	uint64_t m_lastUpdateFrameIndex = 0;
};
