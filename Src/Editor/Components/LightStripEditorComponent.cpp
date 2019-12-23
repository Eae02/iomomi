#include "LightStripEditorComponent.hpp"
#include "../../World/Entities/Components/ActivatableComp.hpp"
#include "../../World/Entities/Components/ActivatorComp.hpp"

void LightStripEditorComponent::Update(float dt, const EditorState& editorState)
{
	
}

bool LightStripEditorComponent::UpdateInput(float dt, const EditorState& editorState)
{
	if (std::shared_ptr<ActivationLightStripEnt> editingLightStripEnt = m_editingLightStripEntity.lock())
	{
		std::shared_ptr<Ent> activatorEnt = editingLightStripEnt->ActivatorEntity().lock();
		ActivatorComp* activator = activatorEnt ? activatorEnt->GetComponentMut<ActivatorComp>() : nullptr;
		
		if (activator != nullptr)
		{
			WallRayIntersectResult pickResult = editorState.world->RayIntersectWall(editorState.viewRay);
			if (pickResult.intersected)
			{
				activator->waypoints[m_editingWayPointIndex] = { pickResult.normalDir, pickResult.intersectPosition };
				ActivationLightStripEnt::GenerateForActivator(*editorState.world, *activatorEnt);
			}
		}
		
		if (!eg::IsButtonDown(eg::Button::MouseLeft) || activator == nullptr)
		{
			m_editingLightStripEntity = { };
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
		//Attempts to get an activator component from the only selected entity
		std::shared_ptr<Ent> activatorEntity = editorState.selectedEntities->front().lock();
		ActivatorComp* activator = activatorEntity ? activatorEntity->GetComponentMut<ActivatorComp>() : nullptr;
		if (activator != nullptr)
		{
			//Adds icons for existing points
			for (size_t i = 0; i < activator->waypoints.size(); i++)
			{
				EditorIcon& icon = icons.emplace_back(activator->waypoints[i].position, [this, activatorEntity, activator, i, selectedEntities = editorState.selectedEntities]
				{
					m_editingLightStripEntity = activator->lightStripEntity;
					m_editingWayPointIndex = i;
					selectedEntities->push_back(activatorEntity);
				});
				icon.iconIndex = 7;
				if (icon.Rectangle().Contains({ eg::CursorX(), eg::CurrentResolutionY() - eg::CursorY() }))
					hoveringExistingPoint = true;
			}
			
			//Adds icons to connect this activator to a new connection point
			editorState.world->entManager.ForEach([&] (Ent& activatableEntity)
			{
				const ActivatableComp* activatableComp = activatableEntity.GetComponent<ActivatableComp>();
				if (eg::HasFlag(activatableEntity.TypeFlags(), EntTypeFlags::EditorInvisible) || activatableComp == nullptr)
					return;
				
				std::vector<glm::vec3> connectionPoints = activatableComp->GetConnectionPoints(activatableEntity);
				for (int i = 0; i < (int)connectionPoints.size(); i++)
				{
					EditorIcon& icon = icons.emplace_back(connectionPoints[i], [activatorEntity, activator, i,
						world = editorState.world, activatableSP = activatableEntity.shared_from_this()]
					{
						ActivatableComp* activatableComp2 = activatableSP->GetComponentMut<ActivatableComp>();
						
						activator->activatableName = 0;
						activatableComp2->SetConnected(i);
						
						activator->waypoints.clear();
						activator->activatableName = activatableComp2->m_name;
						activator->targetConnectionIndex = i;
						ActivationLightStripEnt::GenerateForActivator(*world, *activatorEntity);
					});
					
					icon.iconIndex = 6;
					if (activator->activatableName == activatableComp->m_name && activator->targetConnectionIndex == i)
						icon.selected = true;
				}
			});
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
		
		editorState.world->entManager.ForEachOfType<ActivationLightStripEnt>([&] (const ActivationLightStripEnt& lightStrip)
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
		});
		
		//Add an icon for a new waypoint
		if (closestDist < 0.5f)
		{
			EditorIcon& icon = icons.emplace_back(closestPoint, [this, closestActivatorEntity, nextWayPoint, closestPoint, wallNormal, selectedEntities = editorState.selectedEntities]
			{
				std::shared_ptr<Ent> activatorEntity = closestActivatorEntity.lock();
				if (activatorEntity == nullptr) return;
				ActivatorComp* activator = activatorEntity->GetComponentMut<ActivatorComp>();
				if (activator == nullptr) return;
				
				ActivationLightStripEnt::WayPoint wp;
				wp.position = closestPoint;
				wp.wallNormal = wallNormal;
				activator->waypoints.insert(activator->waypoints.begin() + nextWayPoint, wp);
				
				m_editingLightStripEntity = activator->lightStripEntity;
				m_editingWayPointIndex = nextWayPoint;
				
				selectedEntities->push_back(activatorEntity);
			});
			icon.iconIndex = 7;
		}
	}
	
	return false;
}
