#pragma once

#include "Entity.hpp"

class GravitySwitchEntity : public Entity, public Entity::IDrawable, public Entity::IEditorWallDrag
{
public:
	void Draw(eg::MeshBatch& meshBatch) override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal) override;
	
	void EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir) override;
	
	int GetEditorIconIndex() const override;
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
	Dir Up() const
	{
		return m_up;
	}
	
	void SetUp(Dir up)
	{
		m_up = up;
	}

private:
	glm::mat4 GetTransform() const;
	
	Dir m_up;
};
