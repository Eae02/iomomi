#pragma once

#include <optional>
#include <EGame/AABB.hpp>
#include "../Entity.hpp"
#include "../../PhysicsEngine.hpp"

class RampEnt : public Ent
{
public:
	RampEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Ramp;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::HasPhysics;
	
	void Spawned(bool isEditor) override;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	int m_yaw = 0;
	bool m_flipped = false;
	
	glm::vec3 m_size { 1.0f };
	glm::vec3 m_position;
	
private:
	void InitializeVertexBuffer();
	
	std::array<glm::vec3, 4> GetTransformedVertices() const;
	
	bool m_vertexBufferOutOfDate = false;
	eg::Buffer m_vertexBuffer;
	
	eg::CollisionMesh m_collisionMesh;
	PhysicsObject m_physicsObject;
};

template <>
std::shared_ptr<Ent> CloneEntity<RampEnt>(const Ent& entity);
