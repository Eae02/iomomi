#pragma once

#include "Entity.hpp"

class EntranceEntity : public Entity, public Entity::IDrawable, public Entity::IEditorWallDrag
{
public:
	EntranceEntity();
	
	void Draw(class ObjectRenderer& renderer) override;
	
	int GetEditorIconIndex() const override;
	
	void EditorRenderSettings() override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir) override;
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
private:
	glm::mat4 GetTransform() const;
	
	Dir m_direction = Dir::NegX;
	const eg::Model* m_model;
	std::vector<const class ObjectMaterial*> m_materials;
};
