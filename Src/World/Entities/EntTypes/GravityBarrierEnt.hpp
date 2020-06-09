#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../Components/AxisAlignedQuadComp.hpp"
#include "../../PhysicsEngine.hpp"

struct GravityBarrierInteractableComp
{
	Dir currentDown;
};

class GravityBarrierEnt : public Ent
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::GravityBarrier;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::Activatable | EntTypeFlags::HasPhysics;
	
	GravityBarrierEnt();
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	void Update(const WorldUpdateArgs& args) override;
	
	void Spawned(bool isEditor) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	int BlockedAxis() const;
	
	glm::mat4 GetTransform() const;
	glm::vec3 GetPosition() const override { return m_position; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	enum class ActivateAction
	{
		Disable,
		Enable,
		Rotate
	};
	
	int flowDirection = 0;
	ActivateAction activateAction = ActivateAction::Disable;
	
private:
	glm::vec3 m_position;
	
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);
	
	static bool ShouldCollide(const PhysicsObject& self, const PhysicsObject& other);
	
	std::tuple<glm::vec3, glm::vec3> GetTangents() const;
	
	ActivatableComp m_activatable;
	AxisAlignedQuadComp m_aaQuad;
	
	float m_opacity = 1;
	bool m_enabled = true;
	bool m_blockFalling = false;
	int m_flowDirectionOffset = 0;
	
	PhysicsObject m_physicsObject;
};
