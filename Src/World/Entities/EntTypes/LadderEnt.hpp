#pragma once

#include "../Entity.hpp"

class LadderEnt : public Ent
{
public:
	static constexpr EntTypeID TypeID = EntTypeID::Ladder;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::ShadowDrawableS | EntTypeFlags::EditorWallMove;
	
	LadderEnt();
	
	void RenderSettings() override;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	Dir GetFacingDirection() const override { return m_forward; }
	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	glm::vec3 EdGetGridAlignment() const override;
	
	const eg::AABB& GetAABB() const { return m_aabb; }
	
	const glm::vec3& GetDownVector() const { return m_downVector; }
	
private:
	void UpdateTransformAndAABB();
	
	glm::mat4 m_commonTransform;
	eg::AABB m_aabb;
	glm::vec3 m_downVector;
	
	Dir m_forward = {};
	glm::vec3 m_position;
	
	int m_length = 1;
	int m_downDirection = 0;
};
