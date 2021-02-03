#pragma once

#include "../../Entity.hpp"
#include "../../../PhysicsEngine.hpp"

class MeshEnt : public Ent
{
public:
	MeshEnt();
	
	static constexpr EntTypeID TypeID = EntTypeID::Mesh;
	static constexpr EntTypeFlags EntFlags = EntTypeFlags::Drawable | EntTypeFlags::EditorDrawable |
		EntTypeFlags::ShadowDrawableS | EntTypeFlags::EditorRotatable | EntTypeFlags::HasPhysics;
	
	void Serialize(std::ostream& stream) const override;
	
	void Deserialize(std::istream& stream) override;
	
	void RenderSettings() override;
	
	void CommonDraw(const EntDrawArgs& args) override;
	
	glm::vec3 GetPosition() const override { return m_position; }
	void EditorMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection) override;
	
	glm::quat GetEditorRotation() override;
	void EditorRotated(const glm::quat& newRotation) override;
	
	void CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt) override;
	
	eg::Span<const EditorSelectionMesh> GetEditorSelectionMeshes() const override;
	
private:
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
	
	bool m_hasCollision = false;
	PhysicsObject m_physicsObject;
	
	std::vector<EditorSelectionMesh> m_editorSelectionMeshes;
};
