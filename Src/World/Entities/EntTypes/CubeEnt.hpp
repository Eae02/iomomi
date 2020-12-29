#pragma once

#include "GravityBarrierEnt.hpp"
#include "../Entity.hpp"
#include "../EntInteractable.hpp"
#include "../EntGravityChargeable.hpp"
#include "../../Dir.hpp"
#include "../../../Graphics/WaterSimulator.hpp"
#include "../../PhysicsEngine.hpp"

class CubeEnt : public Ent, public EntInteractable, public EntGravityChargeable
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::Cube;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::ShadowDrawableD | EntTypeFlags::Interactable | EntTypeFlags::HasPhysics;
	
	CubeEnt() : CubeEnt(glm::vec3(0.0f), false) { }
	CubeEnt(const glm::vec3& position, bool canFloat);
	
	glm::vec3 GetPosition() const override { return m_physicsObject.position; }
	
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	int GetEditorIconIndex() const override;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void GameDraw(const EntGameDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	void UpdatePostSim(const struct WorldUpdateArgs& args);
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void Interact(class Player& player) override;
	int CheckInteraction(const class Player& player, const PhysicsEngine& physicsEngine) const override;
	std::optional<InteractControlHint> GetInteractControlHint() const override;
	
	bool SetGravity(Dir newGravity) override;
	
	bool ShouldShowGravityBeamControlHint(Dir newGravity) override;
	
	Dir CurrentDown() const { return m_currentDown; }
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	static constexpr float RADIUS = 0.4f;
	
	eg::Sphere GetSphere() const
	{
		return eg::Sphere(m_physicsObject.position, RADIUS * std::sqrt(3.0f));
	}
	
	bool canFloat = false;
	
private:
	void Draw(eg::MeshBatch& meshBatch, const glm::mat4& transform) const;
	
	static bool ShouldCollide(const PhysicsObject& self, const PhysicsObject& other);
	
	glm::vec3 m_previousPosition;
	PhysicsObject m_physicsObject;
	GravityBarrierInteractableComp m_barrierInteractableComp;
	
	bool m_collisionWithPlayerDisabled = false;
	bool m_showChangeGravityControlHint = false;
	bool m_isPickedUp = false;
	Dir m_currentDown = Dir::NegY;
	
	std::shared_ptr<WaterSimulator::QueryAABB> m_waterQueryAABB;
};
