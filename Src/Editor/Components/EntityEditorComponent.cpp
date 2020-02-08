#include "EntityEditorComponent.hpp"
#include "../../World/Entities/Components/ActivatableComp.hpp"
#include "../../World/Entities/EntTypes/ActivationLightStripEnt.hpp"
#include "../../World/Entities/Components/ActivatorComp.hpp"

#include <imgui.h>

bool EntityEditorComponent::UpdateInput(float dt, const EditorState& editorState)
{
	if (eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
	{
		m_mouseDownPos = eg::CursorPos();
	}
	
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
	
	auto EntityMoved = [&] (Ent& entity)
	{
		if (ActivatableComp* activatable = entity.GetComponentMut<ActivatableComp>())
		{
			editorState.world->entManager.ForEach([&] (Ent& activatorEntity)
			{
				ActivatorComp* activator = activatorEntity.GetComponentMut<ActivatorComp>();
				if (activator && activator->activatableName == activatable->m_name)
				{
					ActivationLightStripEnt::GenerateForActivator(*editorState.world, activatorEntity);
				}
			});
		}
		
		ActivationLightStripEnt::GenerateForActivator(*editorState.world, entity);
	};
	
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
			WallRayIntersectResult pickResult = editorState.world->RayIntersectWall(editorState.viewRay);
			if (pickResult.intersected)
			{
				if (MaybeClone())
					UpdateWallDragEntity();
				
				wallDragEntity->EditorMoved(SnapToGrid(pickResult.intersectPosition), pickResult.normalDir);
				EntityMoved(*wallDragEntity);
			}
		}
		else if (std::abs(m_mouseDownPos.x - eg::CursorX()) > DRAG_BEGIN_DELTA ||
		         std::abs(m_mouseDownPos.y - eg::CursorY()) > DRAG_BEGIN_DELTA)
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
			m_gizmoPosUnaligned = glm::vec3(0.0f);
			for (int64_t i = editorState.selectedEntities->size() - 1; i >= 0; i--)
			{
				std::shared_ptr<Ent> selectedEntitySP = editorState.selectedEntities->at(i).lock();
				if (!selectedEntitySP || eg::HasFlag(selectedEntitySP->TypeFlags(), EntTypeFlags::EditorWallMove))
				{
					editorState.selectedEntities->at(i).swap(editorState.selectedEntities->back());
					editorState.selectedEntities->pop_back();
				}
				else
				{
					m_gizmoPosUnaligned += selectedEntitySP->Pos();
				}
			}
			m_gizmoPosUnaligned /= (float)editorState.selectedEntities->size();
			m_prevGizmoPos = m_gizmoPosUnaligned;
		}
		
		m_drawTranslationGizmo = true;
		m_translationGizmo.Update(m_gizmoPosUnaligned, RenderSettings::instance->cameraPosition,
		                          RenderSettings::instance->viewProjection, editorState.viewRay);
		
		const glm::vec3 gizmoPosAligned = SnapToGrid(m_gizmoPosUnaligned);
		const glm::vec3 dragDelta = gizmoPosAligned - m_prevGizmoPos;
		if (glm::length2(dragDelta) > 1E-6f)
		{
			MaybeClone();
			for (const std::weak_ptr<Ent>& selectedEntity : *editorState.selectedEntities)
			{
				if (std::shared_ptr<Ent> selectedEntitySP = selectedEntity.lock())
				{
					selectedEntitySP->EditorMoved(selectedEntitySP->Pos() + dragDelta, {});
					EntityMoved(*selectedEntitySP);
				}
			}
		}
		m_prevGizmoPos = gizmoPosAligned;
		
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
		
		EditorIcon& icon = icons.emplace_back(entity.Pos(), [entityWP = entity.shared_from_this(), selected = editorState.selectedEntities] ()
		{
			selected->push_back(move(entityWP));
		});
		icon.iconIndex = entTypeMap.at(entity.TypeID()).editorIconIndex;
		icon.selected = editorState.IsEntitySelected(entity);
	});
	
	return false;
}

void EntityEditorComponent::LateDraw() const
{
	if (m_drawTranslationGizmo)
	{
		m_translationGizmo.Draw(RenderSettings::instance->viewProjection);
	}
}
