#pragma once

#include <optional>
#include <EGame/AABB.hpp>
#include "../Entity.hpp"
#include "../Components/RigidBodyComp.hpp"

class RampEnt : public Ent
{
public:
	RampEnt() = default;
	
	static constexpr EntTypeID TypeID = EntTypeID::Ramp;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::HasCollision;
	
	void Spawned(bool isEditor) override;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	const void* GetComponent(const std::type_info& type) const override;
	
	std::optional<glm::vec3> CheckCollision(const eg::AABB& aabb, const glm::vec3& moveDir) const override;
	
	int m_yaw = 0;
	bool m_flipped = false;
	
	glm::vec3 m_size { 1.0f };
	
private:
	void InitializeVertexBuffer();
	
	std::array<glm::vec3, 4> GetTransformedVertices() const;
	
	bool m_vertexBufferOutOfDate = false;
	eg::Buffer m_vertexBuffer;
	
	RigidBodyComp m_rigidBody;
	btTriangleMesh m_collisionMesh;
	std::unique_ptr<btBvhTriangleMeshShape> m_collisionShape;
};

template <>
std::shared_ptr<Ent> CloneEntity<RampEnt>(const Ent& entity);
