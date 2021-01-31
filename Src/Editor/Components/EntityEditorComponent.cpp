#include "EntityEditorComponent.hpp"
#include "../PrimitiveRenderer.hpp"
#include "../../World/Entities/EntTypes/Activation/ActivationLightStripEnt.hpp"
#include "../../World/Entities/Components/ActivatorComp.hpp"
#include "../../World/Entities/EntEditorWallRotate.hpp"
#include "../../ImGuiInterface.hpp"

#include <imgui.h>

EntityEditorComponent::EntityEditorComponent()
{
	m_translationGizmo.size = 0.15f;
	m_rotationGizmo.size = 0.2f;
}

bool EntityEditorComponent::UpdateInput(float dt, const EditorState& editorState)
{
	if (eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		m_mouseDownPos = eg::CursorPos();
	}
	
	if (eg::FrameIdx() != m_lastUpdateFrameIndex + 1)
	{
		m_isDraggingWallEntity = false;
		m_mouseDownPos = {};
	}
	m_lastUpdateFrameIndex = eg::FrameIdx();
	
	auto MaybeClone = [&] ()
	{
		if (!eg::IsButtonDown(eg::Button::LeftShift) || m_entitiesCloned)
			return false;
		
		for (std::weak_ptr<Ent>& entityWeak : *editorState.selectedEntities)
		{
			if (std::shared_ptr<Ent> entity = entityWeak.lock())
			{
				if ((entity = entity->Clone()))
				{
					entityWeak = entity;
					editorState.world->entManager.AddEntity(std::move(entity));
				}
			}
		}
		m_entitiesCloned = true;
		return true;
	};
	
	if (!eg::IsButtonDown(eg::Button::LeftShift))
		m_entitiesCloned = false;
	
	const bool mouseHeld = eg::IsButtonDown(eg::Button::MouseLeft) && eg::WasButtonDown(eg::Button::MouseLeft);
	
	m_wallDragEntity = nullptr;
	auto UpdateWallDragEntity = [&] ()
	{
		if (editorState.selectedEntities->size() == 1)
		{
			m_wallDragEntity = editorState.selectedEntities->at(0).lock();
			if (!eg::HasFlag(m_wallDragEntity->TypeFlags(), EntTypeFlags::EditorWallMove))
				m_wallDragEntity = nullptr;
		}
	};
	UpdateWallDragEntity();
	
	//Moves the wall drag entity
	if (m_wallDragEntity != nullptr && mouseHeld)
	{
		constexpr int DRAG_BEGIN_DELTA = 10;
		if (m_isDraggingWallEntity)
		{
			VoxelRayIntersectResult pickResult = editorState.world->voxels.RayIntersect(editorState.viewRay);
			if (pickResult.intersected)
			{
				if (MaybeClone())
					UpdateWallDragEntity();
				
				glm::vec3 newPos = pickResult.intersectPosition;
				if (eg::InputState::Current().IsAltDown())
					newPos = SnapToGrid(newPos, BIG_GRID_SNAP);
				else if (!eg::InputState::Current().IsCtrlDown())
					newPos = SnapToGrid(newPos, SMALL_GRID_SNAP);
				
				m_wallDragEntity->EditorMoved(newPos, pickResult.normalDir);
				editorState.EntityMoved(*m_wallDragEntity);
			}
		}
		else if (m_mouseDownPos.has_value() &&
			glm::distance2(glm::vec2(*m_mouseDownPos), glm::vec2(eg::CursorPos())) > DRAG_BEGIN_DELTA * DRAG_BEGIN_DELTA)
		{
			m_isDraggingWallEntity = true;
		}
	}
	else
	{
		m_isDraggingWallEntity = false;
	}
	
	bool blockFurtherInput = false;
	
	//Removed invalid selected entities and locks valid ones
	std::vector<std::shared_ptr<Ent>> selectedEntities;
	for (int64_t i = editorState.selectedEntities->size() - 1; i >= 0; i--)
	{
		std::shared_ptr<Ent> selectedEntitySP = editorState.selectedEntities->at(i).lock();
		if (!selectedEntitySP)
		{
			editorState.selectedEntities->at(i).swap(editorState.selectedEntities->back());
			editorState.selectedEntities->pop_back();
		}
		else
		{
			selectedEntities.emplace_back(selectedEntitySP);
		}
	}
	
	auto UpdateGizmoEntities = [&] ()
	{
		m_transformingEntities.resize(selectedEntities.size());
		m_initialGizmoPos = glm::vec3(0.0f);
		for (size_t i = 0; i < selectedEntities.size(); i++)
		{
			const glm::vec3 pos = selectedEntities[i]->GetPosition();
			
			m_transformingEntities[i].entity = selectedEntities[i];
			m_transformingEntities[i].initialPosition = pos;
			m_transformingEntities[i].lastPositionSet = pos;
			m_transformingEntities[i].initialRotation = selectedEntities[i]->GetEditorRotation();
			
			m_initialGizmoPos += pos;
		}
		m_initialGizmoPos /= (float)selectedEntities.size();
		m_gizmoPos = m_initialGizmoPos;
	};
	
	//Updates the translation gizmo
	m_drawTranslationGizmo = false;
	if (!selectedEntities.empty() && m_wallDragEntity == nullptr && !m_inRotationMode)
	{
		if (!m_translationGizmo.HasInputFocus())
		{
			UpdateGizmoEntities();
		}
		
		m_drawTranslationGizmo = true;
		m_translationGizmo.Update(m_gizmoPos, editorState.cameraPosition, editorState.viewProjection, editorState.viewRay);
		
		m_gizmoDragDist = m_gizmoPos - m_initialGizmoPos;
		if (m_translationGizmo.CurrentAxis() != -1)
		{
			if (!eg::InputState::Current().IsAltDown() && !eg::InputState::Current().IsCtrlDown())
			{
				float gridSnap = SMALL_GRID_SNAP;
				if (m_transformingEntities.size() == 1)
				{
					std::shared_ptr<Ent> dragEntity = m_transformingEntities[0].entity.lock();
					if (dragEntity != nullptr)
					{
						gridSnap = dragEntity->GetEditorGridAlignment()[m_translationGizmo.CurrentAxis()];
					}
				}
				m_gizmoDragDist = SnapToGrid(m_gizmoDragDist, gridSnap);
			}
			if (glm::length2(m_gizmoDragDist) > 1E-6f)
			{
				MaybeClone();
			}
			for (TransformingEntity& entity : m_transformingEntities)
			{
				if (std::shared_ptr<Ent> selectedEntitySP = entity.entity.lock())
				{
					glm::vec3 newPos = entity.initialPosition + m_gizmoDragDist;
					if (eg::InputState::Current().IsAltDown())
					{
						newPos[m_translationGizmo.CurrentAxis()] =
							SnapToGrid(newPos[m_translationGizmo.CurrentAxis()], BIG_GRID_SNAP);
					}
					if (glm::distance2(newPos, entity.lastPositionSet) > 1E-6f)
					{
						selectedEntitySP->EditorMoved(newPos, {});
						editorState.EntityMoved(*selectedEntitySP);
						entity.lastPositionSet = newPos;
					}
				}
			}
		}
		
		if (m_translationGizmo.HasInputFocus())
			blockFurtherInput = true;
	}
	
	//Updates m_canRotate
	if (!selectedEntities.empty())
	{
		m_canRotate = false;
		if (m_wallDragEntity == nullptr)
		{
			m_canRotate = true;
			for (const std::shared_ptr<Ent>& entity : selectedEntities)
			{
				if (!eg::HasFlag(entity->TypeFlags(), EntTypeFlags::EditorRotatable))
				{
					m_canRotate = false;
					break;
				}
			}
		}
		
		if (!m_canRotate)
			m_inRotationMode = false;
		m_drawRotationGizmo = m_inRotationMode;
		
		if (m_inRotationMode)
		{
			constexpr float SMALL_ANGLE_SNAP = glm::radians(5.0f);
			constexpr float BIG_ANGLE_SNAP = glm::radians(45.0f);
			
			m_rotationGizmo.dragIncrementRadians = 0;
			if (eg::InputState::Current().IsAltDown())
				m_rotationGizmo.dragIncrementRadians = BIG_ANGLE_SNAP;
			else if (!eg::InputState::Current().IsCtrlDown())
				m_rotationGizmo.dragIncrementRadians = SMALL_ANGLE_SNAP;
			
			if (!m_rotationGizmo.HasInputFocus())
			{
				m_rotationValue = {};
				UpdateGizmoEntities();
			}
			
			m_rotationGizmo.Update(m_rotationValue, m_gizmoPos, editorState.cameraPosition,
			                       editorState.viewProjection, editorState.viewRay);
			
			if (m_rotationGizmo.HasInputFocus())
			{
				for (TransformingEntity& entity : m_transformingEntities)
				{
					if (std::shared_ptr<Ent> entitySP = entity.entity.lock())
					{
						entitySP->EditorRotated(m_rotationValue * entity.initialRotation);
					}
				}
				blockFurtherInput = true;
			}
		}
	}
	else
	{
		m_drawRotationGizmo = false;
	}
	
	if (eg::InputState::Current().IsAltDown())
	{
		if (m_canRotate && eg::IsButtonDown(eg::Button::R) && !eg::WasButtonDown(eg::Button::R))
			m_inRotationMode = true;
		if (eg::IsButtonDown(eg::Button::G) && !eg::WasButtonDown(eg::Button::G))
			m_inRotationMode = false;
	}
	
	//Despawns entities or removes light strip way points if delete is pressed
	if (eg::IsButtonDown(eg::Button::Delete) && !eg::WasButtonDown(eg::Button::Delete))
	{
		for (const std::shared_ptr<Ent>& entity : selectedEntities)
		{
			editorState.world->entManager.RemoveEntity(*entity);
		}
		editorState.selectedEntities->clear();
	}
	
	return blockFurtherInput;
}

void EntityEditorComponent::RenderSettings(const EditorState& editorState)
{
	ImGui::Separator();
	
	if (ImGui::RadioButton("Translate (Alt+G)", !m_inRotationMode))
		m_inRotationMode = false;
	
	ImPushDisabled(!m_canRotate);
	if (ImGui::RadioButton("Rotate (Alt+R)", m_inRotationMode))
		m_inRotationMode = true;
	ImPopDisabled(!m_canRotate);
	
	ImGui::Separator();
	
	for (const std::weak_ptr<Ent>& entityHandle : *editorState.selectedEntities)
	{
		std::shared_ptr<Ent> entity = entityHandle.lock();
		if (entity == nullptr)
			continue;
		
		const EntType& entType = entTypeMap.at(entity->TypeID());
		if (eg::HasFlag(entType.flags, EntTypeFlags::EditorInvisible))
			continue;
		
		ImGui::PushID(entity->Name());
		if (ImGui::CollapsingHeader(entType.prettyName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			entity->RenderSettings();
			
			if (ActivatorComp* activator = entity->GetComponentMut<ActivatorComp>())
			{
				const bool disabled = activator->activatableName == 0;
				if (disabled)
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
				if (ImGui::Button("Disconnect"))
				{
					activator->activatableName = 0;
					if (std::shared_ptr<ActivationLightStripEnt> lightStrip = activator->lightStripEntity.lock())
					{
						editorState.world->entManager.RemoveEntity(*lightStrip);
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Generate"))
				{
					ActivationLightStripEnt::GenerateForActivator(*editorState.world, *entity);
				}
				if (disabled)
					ImGui::PopStyleVar();
			}
		}
		ImGui::PopID();
	}
}

bool EntityEditorComponent::CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons)
{
	editorState.world->entManager.ForEach([&] (Ent& entity)
	{
		if (eg::HasFlag(entity.TypeFlags(), EntTypeFlags::EditorInvisible))
			return;
		
		EditorIcon icon = editorState.CreateIcon(entity.GetPosition(),
			[entityWP = entity.shared_from_this(), selected = editorState.selectedEntities] ()
			{
				selected->push_back(move(entityWP));
			});
		icon.iconIndex = entity.GetEditorIconIndex();
		icon.selected = editorState.IsEntitySelected(entity);
		icons.push_back(std::move(icon));
	});
	
	return false;
}

void EntityEditorComponent::LateDraw(const EditorState& editorState) const
{
	if (m_drawTranslationGizmo)
	{
		m_translationGizmo.Draw(editorState.viewProjection);
	}
	if (m_drawRotationGizmo)
	{
		m_rotationGizmo.Draw(editorState.viewProjection);
	}
}
