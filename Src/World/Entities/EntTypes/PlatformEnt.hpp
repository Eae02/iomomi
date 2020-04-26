#pragma once

#include "../Entity.hpp"
#include "../Components/RigidBodyComp.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../Components/AxisAlignedQuadComp.hpp"
#include "../../PhysicsEngine.hpp"

class PlatformEnt : public Ent
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::Platform;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::EditorWallMove | EntTypeFlags::Activatable | EntTypeFlags::HasCollision;
	
	PlatformEnt();
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	std::optional<glm::vec3> CheckCollision(const eg::AABB& aabb, const glm::vec3& moveDir) const override;
	
	glm::vec3 GetPlatformPosition() const { return GetPlatformPosition(m_slideProgress); }
	glm::vec3 GetPlatformPosition(float slideProgress) const;
	
	eg::AABB GetPlatformAABB() const;
	
	static PlatformEnt* FindPlatform(const eg::AABB& searchAABB, class EntityManager& entityManager);
	
	const glm::vec3& MoveDelta() const { return m_moveDelta; }
	const glm::vec3& LaunchVelocity() const { return m_launchVelocity; }
	
private:
	glm::mat4 GetPlatformTransform(float slideProgress) const;
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);
	
	void ComputeLaunchVelocity();
	
	PhysicsObject m_physicsObject;
	ActivatableComp m_activatable;
	AxisAlignedQuadComp m_aaQuadComp;
	AxisAlignedQuadComp::CollisionGeometry m_collisionGeometry;
	
	float m_slideProgress = 0.0f;
	float m_slideTime = 1.0f;
	float m_launchSpeed = 0.0f;
	glm::vec2 m_slideOffset;
	glm::vec3 m_moveDelta;
	glm::vec3 m_launchVelocity;
	glm::vec3 m_launchVelocityToSet;
};
