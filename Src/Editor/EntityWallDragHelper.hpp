#pragma once

#include "../World/Entity.hpp"

class EntityWallDragHelper
{
public:
	bool EditorInteract(Entity& self, const Entity::EditorInteractArgs& interactArgs);
	
private:
	//const eg::CollisionMesh* m_collisionMesh;
	bool m_isDragging;
};
