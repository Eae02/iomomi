#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatorComp.hpp"
#include "../../PhysicsEngine.hpp"

class FloorButtonEnt : public Ent
{
public:
	FloorButtonEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::FloorButton;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::ShadowDrawableS |
		EntTypeFlags::EditorWallMove | EntTypeFlags::HasPhysics;
	
	void RenderSettings() override;
	
	Dir GetFacingDirection() const override { return m_direction; }
	glm::vec3 GetPosition() const override { return m_ringPhysicsObject.position; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	eg::AABB GetAABB() const;
	
	inline void Activate()
	{
		m_activator.Activate();
	}
	
private:
	static bool ShouldCollide(const PhysicsObject& self, const PhysicsObject& other);
	static glm::vec3 ConstrainMove(const PhysicsObject& object, const glm::vec3& move);
	
	Dir m_direction;
	
	ActivatorComp m_activator;
	PhysicsObject m_ringPhysicsObject;
	PhysicsObject m_padPhysicsObject;
	
	float m_timeSinceActivated = INFINITY;
	float m_lightColor = 0;
	float m_lingerTime = 0;
};
