#include "AAQuadDragEditorComponent.hpp"
#include "../PrimitiveRenderer.hpp"
#include "../../World/Entities/Components/AxisAlignedQuadComp.hpp"

bool AAQuadDragEditorComponent::UpdateInput(float dt, const EditorState& editorState)
{
	std::shared_ptr<Ent> entity = m_activeEntity.lock();
	if (!eg::IsButtonDown(eg::Button::MouseLeft) || !entity)
	{
		m_activeEntity.reset();
		return false;
	}
	
	AxisAlignedQuadComp& quadComp = *entity->GetComponentMut<AxisAlignedQuadComp>();
	eg::Plane plane(quadComp.GetNormal(), entity->Pos());
	
	float planeIntersectDist;
	if (editorState.viewRay.Intersects(plane, planeIntersectDist))
	{
		int dragDim = (quadComp.upPlane + draggingBitangent + 1) % 3;
		
		glm::vec3 intersectPos = editorState.viewRay.GetPoint(planeIntersectDist);
		
		float edges[] = {
			entity->Pos()[dragDim] + quadComp.radius[draggingBitangent] * 0.5f,
			entity->Pos()[dragDim] - quadComp.radius[draggingBitangent] * 0.5f
		};
		edges[draggingNegative] = SnapToGrid(intersectPos[dragDim]);
		
		glm::vec3 newPos = entity->Pos();
		newPos[dragDim] = (edges[0] + edges[1]) / 2.0f;
		quadComp.radius[draggingBitangent] = (edges[0] - edges[1]);
		entity->EditorMoved(newPos, {});
	}
	
	return true;
}

bool AAQuadDragEditorComponent::CollectIcons(const EditorState& editorState, std::vector<EditorIcon>& icons)
{
	std::shared_ptr<Ent> activeEntity = m_activeEntity.lock();
	
	for (const std::weak_ptr<Ent>& entityWP : *editorState.selectedEntities)
	{
		std::shared_ptr<Ent> entity = entityWP.lock();
		AxisAlignedQuadComp* quadComp = entity ? entity->GetComponentMut<AxisAlignedQuadComp>() : nullptr;
		if (quadComp == nullptr || (activeEntity && activeEntity != entity))
			continue;
		
		auto [tangent, bitangent] = quadComp->GetTangents(0);
		glm::vec3 dirs[] = { tangent, -tangent, bitangent, -bitangent };
		
		for (int d = 0; d < 4; d++)
		{
			EditorIcon& icon = icons.emplace_back(entity->Pos() + dirs[d] * 0.5f, [entityWP=entityWP, d, this]
			{
				m_activeEntity = entityWP;
				draggingBitangent = d / 2;
				draggingNegative = d % 2;
			});
			icon.shouldClearSelection = false;
			icon.iconIndex = 9;
		}
	}
	
	return activeEntity != nullptr;
}
