#pragma once

#include "../Entity.hpp"
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
	
	static std::optional<Dir> CheckIntersection(class EntityManager& entityManager, const eg::AABB& aabb);
	
	glm::vec3 radius { 1.0f };
	
	Dir newGravity;
	
private:
	glm::vec3 m_position;
	
	std::vector<ForceFieldParticle> particles;
	float timeSinceEmission = 0;
};
