#pragma once

#include "../Entity.hpp"
#include "../../PhysicsEngine.hpp"

class ColliderEnt : public Ent
{
public:
	ColliderEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Collider;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::HasPhysics | EntTypeFlags::EditorDrawable;
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	glm::vec3 GetPosition() const override { return m_physicsObject.position; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	static bool drawInEditor;
	
private:
	static bool ShouldCollide(const PhysicsObject& self, const PhysicsObject& other);
	
	glm::vec3 m_radius { 1.0f };
	
	bool m_blockPlayer = true;
	bool m_blockCubes = true;
	bool m_blockPickUp = false;
	bool m_blockedGravityModes[6];
	
	PhysicsObject m_physicsObject;
};
