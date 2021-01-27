#include "EntityEditorComponent.hpp"
#include "../../World/Entities/EntTypes/ActivationLightStripEnt.hpp"
#include "../../World/Entities/Components/ActivatorComp.hpp"

#include <imgui.h>
#include <imgui_internal.h>

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
	
	std::shared_ptr<Ent> wallDragEntity;
	auto UpdateWallDragEntity = [&] ()
	{
		if (editorState.selectedEntities->size() == 1)
		{
			wallDragEntity = editorState.selectedEntities->at(0).lock();
			if (!eg::HasFlag(wallDragEntity->TypeFlags(), EntTypeFlags::EditorWallMove))
				wallDragEntity = nullptr;
		}
	};
	UpdateWallDragEntity();
	
	//Moves the wall drag entity
	if (wallDragEntity != nullptr && mouseHeld)
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
				
				wallDragEntity->EditorMoved(newPos, pickResult.normalDir);
				editorState.EntityMoved(*wallDragEntity);
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
	
	//Updates the translation gizmo
	m_drawTranslationGizmo = false;
	if (!editorState.selectedEntities->empty() && wallDragEntity == nullptr)
	{
		if (!m_translationGizmo.HasInputFocus())
		{
			m_dragEntities.clear();
			m_initialGizmoPos = glm::vec3(0.0f);
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
					m_initialGizmoPos += selectedEntitySP->GetPosition();
					m_dragEntities.emplace_back(selectedEntitySP, selectedEntitySP->GetPosition());
				}
			}
			m_initialGizmoPos /= (float)editorState.selectedEntities->size();
			m_gizmoPos = m_initialGizmoPos;
		}
		
		m_drawTranslationGizmo = true;
		m_translationGizmo.Update(m_gizmoPos, editorState.cameraPosition, editorState.viewProjection, editorState.viewRay);
		
		m_gizmoDragDist = m_gizmoPos - m_initialGizmoPos;
		if (m_translationGizmo.CurrentAxis() != -1)
		{
			if (!eg::InputState::Current().IsAltDown() && !eg::InputState::Current().IsCtrlDown())
			{
				float gridSnap = SMALL_GRID_SNAP;
				if (m_dragEntities.size() == 1)
				{
					std::shared_ptr<Ent> dragEntity = m_dragEntities[0].entity.lock();
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
			for (DraggingEntity& entity : m_dragEntities)
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
	
	//Despawns entities or removes light strip way points if delete is pressed
	if (eg::IsButtonDown(eg::Button::Delete) && !eg::WasButtonDown(eg::Button::Delete))
	{
		for (const std::weak_ptr<Ent>& entity : *editorState.selectedEntities)
		{
			std::shared_ptr<Ent> entitySP = entity.lock();
			if (entitySP)
			{
				editorState.world->entManager.RemoveEntity(*entitySP);
			}
		}
		editorState.selectedEntities->clear();
	}
	
	return blockFurtherInput;
}

void EntityEditorComponent::RenderSettings(const EditorState& editorState)
{
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
}
