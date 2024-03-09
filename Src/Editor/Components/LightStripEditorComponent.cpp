#include "LightStripEditorComponent.hpp"

#include "../../World/Entities/Components/ActivatableComp.hpp"
#include "../../World/Entities/Components/ActivatorComp.hpp"

bool LightStripEditorComponent::UpdateInput(float dt, const EditorState& editorState)
{
	if (std::shared_ptr<ActivationLightStripEnt> editingLightStripEnt = m_editingLightStripEntity.lock())
	{
		std::shared_ptr<Ent> activatorEnt = editingLightStripEnt->ActivatorEntity().lock();
		ActivatorComp* activator = activatorEnt ? activatorEnt->GetComponentMut<ActivatorComp>() : nullptr;

		if (activator != nullptr)
		{
			VoxelRayIntersectResult pickResult = editorState.world->voxels.RayIntersect(editorState.viewRay);
			if (pickResult.intersected)
			{
				activator->waypoints[m_editingWayPointIndex] = { pickResult.normalDir, pickResult.intersectPosition };
				ActivationLightStripEnt::GenerateForActivator(*editorState.world, *activatorEnt);
			}
		}

		if (!eg::IsButtonDown(eg::Button::MouseLeft) || activator == nullptr)
		{
			m_editingLightStripEntity = {};
			m_editingWayPointIndex = -1;
		}

		return true;
	}

	return false;
}

bool LightStripEditorComponent::CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons)
{
	bool hoveringExistingPoint = false;

	if (editorState.selectedEntities->size() == 1)
	{
		// Attempts to get an activator component from the only selected entity
		std::shared_ptr<Ent> activatorEntity = editorState.selectedEntities->front().lock();
		ActivatorComp* activator = activatorEntity ? activatorEntity->GetComponentMut<ActivatorComp>() : nullptr;
		if (activator != nullptr)
		{
			// Adds icons for existing points
			for (size_t i = 0; i < activator->waypoints.size(); i++)
			{
				EditorIcon icon = editorState.CreateIcon(
					activator->waypoints[i].position,
					[this, activatorEntity, activator, i]
					{
						m_editingLightStripEntity = activator->lightStripEntity;
						m_editingWayPointIndex = eg::ToInt(i);
					}
				);
				icon.shouldClearSelection = false;
				icon.iconIndex = 7;
				if (icon.Rectangle().Contains(editorState.windowCursorPos))
					hoveringExistingPoint = true;
				icons.push_back(std::move(icon));
			}

			// Adds icons to connect this activator to a new connection point
			editorState.world->entManager.ForEach(
				[&](Ent& activatableEntity)
				{
					if (eg::HasFlag(activatableEntity.TypeFlags(), EntTypeFlags::EditorInvisible))
						return;

					const ActivatableComp* activatableComp = activatableEntity.GetComponent<ActivatableComp>();
					if (activatableComp == nullptr)
						return;

					std::vector<glm::vec3> connectionPoints = activatableComp->GetConnectionPoints(activatableEntity);
					for (int i = 0; i < eg::ToInt(connectionPoints.size()); i++)
					{
						EditorIcon icon = editorState.CreateIcon(
							connectionPoints[i],
							[activatorEntity, activator, i, world = editorState.world,
					         activatableSP = activatableEntity.shared_from_this()]
							{
								ActivatableComp* activatableComp2 = activatableSP->GetComponentMut<ActivatableComp>();

								activator->activatableName = 0;
								activatableComp2->SetConnected(i);

								activator->waypoints.clear();
								activator->activatableName = activatableComp2->m_name;
								activator->targetConnectionIndex = i;
								ActivationLightStripEnt::GenerateForActivator(*world, *activatorEntity);
							}
						);

						icon.ApplyDepthBias(0.01f);

						icon.iconIndex = 6;
						icon.shouldClearSelection = false;
						if (activator->activatableName == activatableComp->m_name &&
					        activator->targetConnectionIndex == i)
							icon.selected = true;

						icons.push_back(std::move(icon));
					}
				}
			);
		}
	}

	if (m_editingLightStripEntity.lock())
		return true;

	if (!hoveringExistingPoint)
	{
		glm::vec3 closestPoint;
		float closestDist = INFINITY;
		std::weak_ptr<Ent> closestActivatorEntity;
		int nextWayPoint;
		Dir wallNormal;

		editorState.world->entManager.ForEachOfType<ActivationLightStripEnt>(
			[&](const ActivationLightStripEnt& lightStrip)
			{
				std::shared_ptr<Ent> activatorEnt = lightStrip.ActivatorEntity().lock();
				if (activatorEnt != nullptr && editorState.IsEntitySelected(*activatorEnt))
				{
					for (size_t i = 1; i < lightStrip.Path().size(); i++)
					{
						const auto& a = lightStrip.Path()[i];
						const auto& b = lightStrip.Path()[i - 1];

						if (a.prevWayPoint != b.prevWayPoint)
							continue;

						eg::Ray lineRay(a.position, b.position - a.position);
						float closestA = lineRay.GetClosestPoint(editorState.viewRay);
						if (!std::isnan(closestA))
						{
							glm::vec3 newPoint = lineRay.GetPoint(glm::clamp(closestA, 0.0f, 1.0f));
							float newDist = editorState.viewRay.GetDistanceToPoint(newPoint);
							if (newDist < closestDist)
							{
								closestDist = newDist;
								closestPoint = newPoint;
								closestActivatorEntity = activatorEnt;
								nextWayPoint = a.prevWayPoint;
								wallNormal = a.wallNormal;
							}
						}
					}
				}
			}
		);

		// Add an icon for a new waypoint
		if (closestDist < 0.5f)
		{
			EditorIcon icon = editorState.CreateIcon(
				closestPoint,
				[this, closestActivatorEntity, nextWayPoint, closestPoint, wallNormal,
			     selectedEntities = editorState.selectedEntities]
				{
					std::shared_ptr<Ent> activatorEntity = closestActivatorEntity.lock();
					if (activatorEntity == nullptr)
						return;
					ActivatorComp* activator = activatorEntity->GetComponentMut<ActivatorComp>();
					if (activator == nullptr)
						return;

					ActivationLightStripEnt::WayPoint wp;
					wp.position = closestPoint;
					wp.wallNormal = wallNormal;
					activator->waypoints.insert(activator->waypoints.begin() + nextWayPoint, wp);

					m_editingLightStripEntity = activator->lightStripEntity;
					m_editingWayPointIndex = nextWayPoint;

					selectedEntities->push_back(activatorEntity);
				}
			);
			icon.hideIfNotHovered = true;
			icon.iconIndex = 7;

			icons.push_back(std::move(icon));
		}
	}

	return false;
}
