#pragma once

#include "../../Entity.hpp"
#include "../../../PhysicsEngine.hpp"

class MeshEnt : public Ent
{
public:
	MeshEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Mesh;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::ShadowDrawableS | EntTypeFlags::EditorRotatable | EntTypeFlags::HasPhysics |
		EntTypeFlags::OptionalEditorIcon;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	void EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	glm::quat EdGetRotation() const override;
	void EdRotated(const glm::quat& newRotation) override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	std::span<const EditorSelectionMesh> EdGetSelectionMeshes() const override;
	
private:
	glm::mat4 GetCommonTransform() const;
	
	template <typename CallbackFn>
	void IterateRepeatedInstances(CallbackFn callback) const;
	
	void UpdateEditorSelectionMeshes();
	
	glm::vec3 m_position;
	glm::vec3 m_scale;
	glm::quat m_rotation;
	
	std::optional<uint32_t> m_model = 0;
	uint32_t m_material = 0;
	int m_numRepeats = 0;
	float m_randomTextureOffset = 0;
	
	bool m_hasCollision = true;
	
	eg::CollisionMesh m_collisionMesh;
	std::optional<PhysicsObject> m_physicsObject;
	
	std::vector<EditorSelectionMesh> m_editorSelectionMeshes;
};
