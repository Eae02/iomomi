#include "EntityEditorComponent.hpp"

#include "../../ImGui.hpp"
#include "../../Settings.hpp"
#include "../../World/Entities/Components/ActivatorComp.hpp"
#include "../../World/Entities/EntTypes/Activation/ActivationLightStripEnt.hpp"
#include "../PrimitiveRenderer.hpp"
#include "../SelectionRenderer.hpp"

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

	auto MaybeClone = [&]()
	{
		if (!eg::IsButtonDown(eg::Button::LeftShift) || m_entitiesCloned)
			return false;

		for (std::weak_ptr<Ent>& entityWeak : *editorState.selectedEntities)
		{
			if (std::shared_ptr<Ent> entity = entityWeak.lock())
			{
				if (std::shared_ptr<Ent> clonedEntity = entity->Clone())
				{
					editorState.world->entManager.AddEntity(std::move(clonedEntity));
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
	auto UpdateWallDragEntity = [&]()
	{
		if (editorState.selectedEntities->size() == 1)
		{
			m_wallDragEntity = editorState.selectedEntities->at(0).lock();
			if (!eg::HasFlag(m_wallDragEntity->TypeFlags(), EntTypeFlags::EditorWallMove))
				m_wallDragEntity = nullptr;
		}
	};
	UpdateWallDragEntity();

	// Moves the wall drag entity
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

				m_wallDragEntity->EdMoved(newPos, pickResult.normalDir);
				editorState.EntityMoved(*m_wallDragEntity);
			}
		}
		else if (
			m_mouseDownPos.has_value() && glm::distance2(glm::vec2(*m_mouseDownPos), glm::vec2(eg::CursorPos())) >
											  DRAG_BEGIN_DELTA * DRAG_BEGIN_DELTA)
		{
			m_isDraggingWallEntity = true;
		}
	}
	else
	{
		m_isDraggingWallEntity = false;
	}

	bool blockFurtherInput = false;

	// Removes invalid selected entities and locks valid ones
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

	// Updates box resize
	if (!blockFurtherInput && m_boxResizeActive)
	{
		if (m_boxResizeEntity == nullptr || !eg::IsButtonDown(eg::Button::MouseLeft))
		{
			m_boxResizeActive = false;
		}
		else
		{
			float dragDist = m_boxResizeDragRay.GetClosestPoint(editorState.viewRay) - m_boxResizeInitialRayPos +
			                 m_boxResizeBeginSize;
			if (!std::isnan(dragDist))
			{
				dragDist = SnapToGrid(std::max(dragDist, 0.0f));
				if (!eg::FEqual(dragDist, m_boxResizeAssignedSize))
				{
					glm::vec3 newSize = m_boxResizeEntity->EdGetSize();
					newSize[m_boxResizeAxis / 2] = (m_boxResizeBeginSize + dragDist) / 2;
					m_boxResizeEntity->EdResized(newSize);
					m_boxResizeEntity->EdMoved(
						m_boxResizeDragRay.GetPoint((dragDist - m_boxResizeBeginSize) / 2.0f), {});
					m_boxResizeAssignedSize = dragDist;
				}
			}
			blockFurtherInput = true;
		}
	}

	auto UpdateGizmoEntities = [&]()
	{
		m_transformingEntities.resize(selectedEntities.size());
		m_initialGizmoPos = glm::vec3(0.0f);
		for (size_t i = 0; i < selectedEntities.size(); i++)
		{
			const glm::vec3 pos = selectedEntities[i]->GetPosition();

			m_transformingEntities[i].entity = selectedEntities[i];
			m_transformingEntities[i].initialPosition = pos;
			m_transformingEntities[i].lastPositionSet = pos;
			m_transformingEntities[i].initialRotation = selectedEntities[i]->EdGetRotation();

			m_initialGizmoPos += pos;
		}
		m_initialGizmoPos /= static_cast<float>(selectedEntities.size());
		m_gizmoPos = m_initialGizmoPos;
	};

	// Updates the translation gizmo
	m_drawTranslationGizmo = false;
	if (!selectedEntities.empty() && m_wallDragEntity == nullptr && !m_inRotationMode && !blockFurtherInput)
	{
		if (!m_translationGizmo.HasInputFocus())
		{
			UpdateGizmoEntities();
		}

		m_drawTranslationGizmo = true;
		m_translationGizmo.Update(
			m_gizmoPos, editorState.cameraPosition, editorState.viewProjection, editorState.viewRay);

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
						gridSnap = dragEntity->EdGetGridAlignment()[m_translationGizmo.CurrentAxis()];
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
						selectedEntitySP->EdMoved(newPos, {});
						editorState.EntityMoved(*selectedEntitySP);
						entity.lastPositionSet = newPos;
					}
				}
			}
		}

		if (m_translationGizmo.HasInputFocus() || m_translationGizmo.IsHovered())
		{
			blockFurtherInput = true;
		}
	}

	// Updates m_canRotate and the rotation gizmo
	if (!selectedEntities.empty())
	{
		m_canRotate = false;
		if (m_wallDragEntity == nullptr && !blockFurtherInput)
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

			m_rotationGizmo.Update(
				m_rotationValue, m_gizmoPos, editorState.cameraPosition, editorState.viewProjection,
				editorState.viewRay);

			if (m_rotationGizmo.HasInputFocus())
			{
				for (TransformingEntity& entity : m_transformingEntities)
				{
					if (std::shared_ptr<Ent> entitySP = entity.entity.lock())
					{
						entitySP->EdRotated(m_rotationValue * entity.initialRotation);
					}
				}
			}

			if (m_rotationGizmo.HasInputFocus() || m_rotationGizmo.IsHovered())
			{
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

	// Despawns entities or removes light strip way points if delete is pressed
	if (eg::IsButtonDown(eg::Button::Delete) && !eg::WasButtonDown(eg::Button::Delete))
	{
		for (const std::shared_ptr<Ent>& entity : selectedEntities)
		{
			editorState.world->entManager.RemoveEntity(*entity);
		}
		editorState.selectedEntities->clear();
	}

	// Updates the hovered selection mesh entity
	m_hoveredSelMeshEntity = {};
	if (!m_boxResizeActive)
	{
		m_boxResizeEntity = {};
		m_boxResizeAxis = -1;
	}
	if (!blockFurtherInput && !editorState.anyIconHovered)
	{
		const VoxelRayIntersectResult voxelRayIR = editorState.world->voxels.RayIntersect(editorState.viewRay);
		float closestIntersect = voxelRayIR.intersected ? voxelRayIR.intersectDist + 0.5f : INFINITY;
		bool closestIsBoxResizeEntity = false;
		bool closestIsSelMeshEntity = false;
		editorState.world->entManager.ForEach(
			[&](Ent& entity)
			{
				if (eg::HasFlag(entity.TypeFlags(), EntTypeFlags::EditorInvisible))
					return;

				bool isSelected = editorState.IsEntitySelected(entity);
				if (!eg::InputState::Current().IsAltDown() && entity.EdGetBoxColor(isSelected))
				{
					auto [entInterDist, axis, axisDir] = PickEntityBoxResize(editorState.viewRay, entity);
					if (axis != -1 && entInterDist < closestIntersect)
					{
						closestIntersect = entInterDist;
						if (eg::HasFlag(entity.TypeFlags(), EntTypeFlags::EditorBoxResizable) && isSelected)
						{
							m_boxResizeAxis = axis;
							m_boxResizeEntity = entity.shared_from_this();
							m_boxResizeDragRay = eg::Ray(entity.GetPosition(), axisDir);
							closestIsBoxResizeEntity = true;
							closestIsSelMeshEntity = false;
						}
						else
						{
							m_hoveredSelMeshEntity = entity.shared_from_this();
							closestIsBoxResizeEntity = false;
							closestIsSelMeshEntity = true;
						}
					}
				}

				for (const EditorSelectionMesh& selMesh : entity.EdGetSelectionMeshes())
				{
					float intersectDist;
					if (selMesh.collisionMesh->Intersect(editorState.viewRay, intersectDist, &selMesh.transform))
					{
						if (intersectDist < closestIntersect)
						{
							m_hoveredSelMeshEntity = entity.shared_from_this();
							closestIntersect = intersectDist;
							closestIsBoxResizeEntity = false;
							closestIsSelMeshEntity = true;
						}
					}
				}
			});

		if (!closestIsSelMeshEntity)
			m_hoveredSelMeshEntity = {};
		if (!closestIsBoxResizeEntity)
			m_boxResizeEntity = {};

		if (eg::IsButtonDown(eg::Button::MouseLeft) && !eg::WasButtonDown(eg::Button::MouseLeft))
		{
			if (closestIsSelMeshEntity)
			{
				if (!eg::InputState::Current().IsCtrlDown())
					editorState.selectedEntities->clear();
				if (!editorState.IsEntitySelected(*m_hoveredSelMeshEntity))
					editorState.selectedEntities->push_back(m_hoveredSelMeshEntity);
				blockFurtherInput = true;
			}
			else if (closestIsBoxResizeEntity)
			{
				m_boxResizeInitialRayPos = m_boxResizeDragRay.GetClosestPoint(editorState.viewRay);
				if (!std::isnan(m_boxResizeInitialRayPos))
				{
					m_boxResizeActive = true;
					m_boxResizeBeginSize = m_boxResizeEntity->EdGetSize()[m_boxResizeAxis / 2];
					m_boxResizeAssignedSize = -1;
				}
				blockFurtherInput = true;
			}
		}
	}

	return blockFurtherInput;
}

std::tuple<float, int, glm::vec3> EntityEditorComponent::PickEntityBoxResize(
	const eg::Ray& viewRay, const Ent& entity) const
{
	glm::vec3 pos = entity.GetPosition();
	glm::vec3 size = entity.EdGetSize();
	glm::quat rot = entity.EdGetRotation();

	glm::vec3 vertices[8];
	for (int i = 0; i < 8; i++)
	{
		vertices[i] = rot * (size * glm::vec3(cubeMesh::vertices[i])) + pos;
	}

	float minDist = INFINITY;
	int axis = -1;
	glm::vec3 axisDir;

	for (int d = 0; d < 6; d++)
	{
		glm::vec3 faceMid;
		glm::vec3 faceVertices[4];
		for (int i = 0; i < 4; i++)
		{
			faceVertices[i] = vertices[cubeMesh::faces[d][i]];
			faceMid += faceVertices[i];
		}
		faceMid /= 4.0f;

		eg::Plane plane(faceVertices[0], faceVertices[1], faceVertices[2]);
		float dist;
		if (viewRay.Intersects(plane, dist) && dist < minDist)
		{
			glm::vec3 point = viewRay.GetPoint(dist);
			if (eg::TriangleContainsPoint(faceVertices[0], faceVertices[1], faceVertices[2], point) ||
			    eg::TriangleContainsPoint(faceVertices[1], faceVertices[2], faceVertices[3], point))
			{
				minDist = dist;
				axis = d;
				axisDir = glm::normalize(faceMid - pos);
			}
		}
	}

	return std::make_tuple(minDist, axis, axisDir);
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

		const EntType* entType = Ent::GetTypeByID(entity->TypeID());
		if (entType == nullptr || eg::HasFlag(entType->flags, EntTypeFlags::EditorInvisible))
			continue;

		ImGui::PushID(entity->Name());
		if (ImGui::CollapsingHeader(entType->prettyName.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
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
	editorState.world->entManager.ForEach(
		[&](Ent& entity)
		{
			if (eg::HasFlag(entity.TypeFlags(), EntTypeFlags::EditorInvisible))
				return;

			if (eg::HasFlag(entity.TypeFlags(), EntTypeFlags::OptionalEditorIcon) &&
		        !settings.edEntityIconEnabled.at(static_cast<int>(entity.TypeID())))
			{
				return;
			}

			const int iconIndex = entity.EdGetIconIndex();
			if (iconIndex == -1)
				return;

			EditorIcon icon = editorState.CreateIcon(
				entity.GetPosition(),
				[entityWP = entity.shared_from_this(), es = &editorState]()
				{
					if (!es->IsEntitySelected(*entityWP))
					{
						es->selectedEntities->push_back(move(entityWP));
					}
				});
			icon.iconIndex = iconIndex;
			icon.selected = editorState.IsEntitySelected(entity);
			icons.push_back(std::move(icon));
		});

	return false;
}

void EntityEditorComponent::DrawEntityBox(
	PrimitiveRenderer& primitiveRenderer, const Ent& entity, bool isSelected) const
{
	std::optional<eg::ColorSRGB> color = entity.EdGetBoxColor(isSelected);
	if (!color.has_value())
		return;
	eg::ColorLin colorLin = *color;

	glm::vec3 pos = entity.GetPosition();
	glm::vec3 size = entity.EdGetSize();
	glm::quat rot = entity.EdGetRotation();

	glm::vec3 vertices[8];
	for (int i = 0; i < 8; i++)
	{
		vertices[i] = rot * (size * glm::vec3(cubeMesh::vertices[i])) + pos;
	}

	for (int d = 0; d < 6; d++)
	{
		float brightness = 0;
		if (m_hoveredSelMeshEntity.get() == &entity || m_boxResizeEntity.get() == &entity)
		{
			brightness = 0.2f;
		}
		if (m_boxResizeEntity.get() == &entity && isSelected && m_boxResizeAxis == d)
		{
			brightness = m_boxResizeActive ? 0.8f : 0.6f;
		}

		glm::vec3 faceVertices[4];
		for (int i = 0; i < 4; i++)
			faceVertices[i] = vertices[cubeMesh::faces[d][i]];
		primitiveRenderer.AddQuad(faceVertices, eg::ColorLin::Mix(colorLin, eg::ColorLin(1, 1, 1, 1), brightness));
	}

	eg::ColorSRGB edgeColor = *color;
	edgeColor.a = glm::mix(edgeColor.a, 1.0f, 0.5f);
	for (std::pair<int, int> edge : cubeMesh::edges)
	{
		primitiveRenderer.AddLine(vertices[edge.first], vertices[edge.second], edgeColor, 0.02f);
	}
}

void EntityEditorComponent::EarlyDraw(const EditorState& editorState) const
{
	// Draws selection outlines around entities with selection meshes
	bool hasDrawnOutlineForHoveredEntity = false;
	for (const std::weak_ptr<Ent>& entityHandle : *editorState.selectedEntities)
	{
		std::shared_ptr<Ent> entity = entityHandle.lock();
		if (entity == nullptr || eg::HasFlag(entity->TypeFlags(), EntTypeFlags::EditorInvisible))
			continue;
		if (entity == m_hoveredSelMeshEntity)
			hasDrawnOutlineForHoveredEntity = true;

		for (const EditorSelectionMesh& selMesh : entity->EdGetSelectionMeshes())
		{
			editorState.selectionRenderer->Draw(1, selMesh.transform, *selMesh.model, selMesh.meshIndex);
		}
	}
	if (m_hoveredSelMeshEntity != nullptr && !hasDrawnOutlineForHoveredEntity)
	{
		for (const EditorSelectionMesh& selMesh : m_hoveredSelMeshEntity->EdGetSelectionMeshes())
		{
			editorState.selectionRenderer->Draw(0.5f, selMesh.transform, *selMesh.model, selMesh.meshIndex);
		}
	}
	editorState.selectionRenderer->EndDraw();
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
