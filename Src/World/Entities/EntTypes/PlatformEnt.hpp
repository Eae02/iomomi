#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../Components/AxisAlignedQuadComp.hpp"
#include "../../PhysicsEngine.hpp"

class PlatformEnt : public Ent
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::Platform;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::EditorWallMove | EntTypeFlags::Activatable | EntTypeFlags::HasPhysics;
	
	PlatformEnt();
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void GameDraw(const EntGameDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	const glm::vec3& LaunchVelocity() const { return m_launchVelocity; }
	glm::vec3 FinalPosition() const;
	
	glm::vec3 GetPosition() const override { return m_basePosition; }
	Dir GetFacingDirection() const override { return m_forwardDir; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
private:
	glm::mat4 GetBaseTransform() const;
	
	static glm::vec3 ConstrainMove(const PhysicsObject& object, const glm::vec3& move);
	
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);
	
	void Draw(eg::MeshBatch& meshBatch, const glm::mat4& transform) const;
	
	void ComputeLaunchVelocity();
	
	glm::vec3 m_basePosition;
	Dir m_forwardDir;
	
	PhysicsObject m_physicsObject;
	ActivatableComp m_activatable;
	AxisAlignedQuadComp m_aaQuadComp;
	
	float m_slideTime = 1.0f;
	float m_launchSpeed = 0.0f;
	glm::vec2 m_slideOffset;
	glm::vec3 m_launchVelocity;
	glm::vec3 m_launchVelocityToSet;
};
