#pragma once

#include "../Entity.hpp"
#include "../Components/AxisAlignedQuadComp.hpp"
#include "../../PhysicsEngine.hpp"
#include "../Components/WaterBlockComp.hpp"
#include "../EntGravityChargeable.hpp"

class SlidingWallEnt : public Ent, public EntGravityChargeable
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::SlidingWall;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::HasPhysics;
	
	SlidingWallEnt();
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	bool SetGravity(Dir newGravity) override;
	
	glm::vec3 GetPosition() const override;
	
	void Spawned(bool isEditor) override;

private:
	static glm::vec3 ConstrainMove(const PhysicsObject& object, const glm::vec3& move);
	
	void UpdateWaterBlockComponent();
	void UpdateShape();
	
	PhysicsObject m_physicsObject;
	AxisAlignedQuadComp m_aaQuadComp;
	WaterBlockComp m_waterBlockComp;
	
	glm::vec3 m_initialPosition;
	glm::vec3 m_slideOffset;
	
	glm::vec3 m_lastShadowUpdatePosition;
	
	bool m_neverBlockWater = false;
	
	Dir m_currentDown = Dir::NegY;
	int m_upPlane = 1;
	
	glm::vec3 m_aabbRadius;
	glm::mat4 m_rotationAndScale;
};
