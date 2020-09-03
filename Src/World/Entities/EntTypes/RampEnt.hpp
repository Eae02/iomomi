#pragma once

#include <optional>
#include <EGame/AABB.hpp>
#include "../Entity.hpp"
#include "../../../Graphics/Materials/DecalMaterial.hpp"
#include "../../PhysicsEngine.hpp"

class RampEnt : public Ent
{
public:
	RampEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Ramp;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable | EntTypeFlags::HasPhysics;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	int m_rotation = 0;
	bool m_flipped = false;
	bool m_hasEdgeDecals = true;
	bool m_stretchTextureV = false;
	float m_textureScale = 1;
	
	glm::vec3 m_size { 1.0f };
	glm::vec3 m_position;
	
private:
	void InitializeVertexBuffer();
	
	glm::mat4 GetTransformationMatrix() const;
	std::array<glm::vec3, 4> GetTransformedVertices(const glm::mat4& matrix) const;
	
	bool m_meshOutOfDate = false;
	eg::Buffer m_vertexBuffer;
	
	uint32_t m_material = 0;
	float m_rampLength = 0;
	
	std::vector<DecalMaterial::InstanceData> m_edgeDecalInstances;
	
	eg::CollisionMesh m_collisionMesh;
	PhysicsObject m_physicsObject;
};

template <>
std::shared_ptr<Ent> CloneEntity<RampEnt>(const Ent& entity);
