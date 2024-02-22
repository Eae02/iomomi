#pragma once

#include <optional>

#include "../../../../Graphics/Materials/DecalMaterial.hpp"
#include "../../../PhysicsEngine.hpp"
#include "../../Entity.hpp"

class RampEnt : public Ent
{
public:
	RampEnt();

	static constexpr EntTypeID TypeID = EntTypeID::Ramp;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
	                                         EntTypeFlags::ShadowDrawableS | EntTypeFlags::HasPhysics |
	                                         EntTypeFlags::EditorBoxResizable;

	void Serialize(std::ostream& stream) const override;

	void Deserialize(std::istream& stream) override;

	void RenderSettings() override;

	void CommonDraw(const EntDrawArgs& args) override;

	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;

	glm::vec3 GetPosition() const override { return m_position; }
	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;

	std::optional<eg::ColorSRGB> EdGetBoxColor(bool selected) const override;

	glm::vec3 EdGetSize() const override;
	glm::quat EdGetRotation() const override;

	void EdResized(const glm::vec3& newSize) override;

	int m_rotation = 0;
	bool m_flipped = false;
	bool m_stretchTextureV = false;
	float m_textureScale = 1;
	int m_textureRotation = 0;
	uint32_t m_material = 0;

	glm::vec3 m_size{ 1.0f };
	glm::vec3 m_position;

private:
	void InitializeVertexBuffer();

	std::pair<glm::vec3, float> GetAngleAxisRotation() const;
	glm::mat4 GetTransformationMatrix() const;
	std::array<glm::vec3, 4> GetTransformedVertices(const glm::mat4& matrix) const;

	bool m_meshOutOfDate = false;
	eg::Buffer m_vertexBuffer;
	std::unique_ptr<eg::MeshBuffersDescriptor> m_meshBuffersDescriptor;

	float m_rampLength = 0;

	eg::CollisionMesh m_collisionMesh;
	PhysicsObject m_physicsObject;
};

template <>
std::shared_ptr<Ent> CloneEntity<RampEnt>(const Ent& entity);
