#pragma once

#include "../Entity.hpp"
#include "../EntCollidable.hpp"
#include "../Components/RigidBodyComp.hpp"
#include "../Components/ActivatableComp.hpp"

class PlatformEnt : public Ent, public EntCollidable
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::Platform;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::EditorWallMove | EntTypeFlags::HasCollision | EntTypeFlags::Activatable;
	
	PlatformEnt();
	
	void Serialize(std::ostream& stream) const override;
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void Draw(const EntDrawArgs& args) override;
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	std::pair<bool, float> RayIntersect(const eg::Ray& ray) const override;
	void CalculateCollision(Dir currentDown, struct ClippingArgs& args) const override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	glm::vec3 GetPlatformPosition() const;
	
	eg::AABB GetPlatformAABB() const;
	
	static PlatformEnt* FindPlatform(const eg::AABB& searchAABB, class EntityManager& entityManager);
	
	const glm::vec3& MoveDelta() const { return m_moveDelta; }
	
private:
	glm::mat4 GetPlatformTransform() const;
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);
	
	void DrawGeneral(eg::MeshBatch& meshBatch) const;
	
	RigidBodyComp m_rigidBody;
	ActivatableComp m_activatable;
	
	float m_slideProgress = 0.0f;
	float m_slideTime = 1.0f;
	glm::vec2 m_slideOffset;
	glm::vec3 m_moveDelta;
};


