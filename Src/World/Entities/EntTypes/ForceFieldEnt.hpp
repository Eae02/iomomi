#pragma once

#include "../Entity.hpp"
#include "../Components/ActivatableComp.hpp"
#include "../../Dir.hpp"

struct ForceFieldSetGravity
{
	Dir newGravity;
};

struct ForceFieldParticle
{
	glm::vec3 start;
	float elapsedTime;
};

class ForceFieldEnt : public Ent
{
public:
	ForceFieldEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::ForceField;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void GameDraw(const EntGameDrawArgs& args) override;
	
	void EditorDraw(const EntEditorDrawArgs& args) override;
	
	void Update(const struct WorldUpdateArgs& args) override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	static std::optional<Dir> CheckIntersection(class EntityManager& entityManager, const eg::AABB& aabb);
	
	glm::vec3 radius { 1.0f };
	
	Dir newGravity;
	
	enum class ActivateAction
	{
		Enable,
		Disable,
		Flip
	};
	
	ActivateAction activateAction = ActivateAction::Enable;
	
private:
	static std::vector<glm::vec3> GetConnectionPoints(const Ent& entity);
	
	glm::vec3 m_position;
	
	Dir m_effectiveNewGravity;
	bool m_enabled = true;
	
	ActivatableComp m_activatable;
	
	std::vector<ForceFieldParticle> m_particles;
	float m_timeSinceEmission = 0;
};
