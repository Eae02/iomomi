#pragma once

#include "Entity.hpp"

class EntranceEntity : public Entity, public Entity::IDrawable
{
public:
	EntranceEntity();
	
	void Draw(class ObjectRenderer& renderer) override;
	
	int GetEditorIconIndex() const override;
	
	void EditorRenderSettings() override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
private:
	glm::mat4 GetTransform() const;
	
	const eg::Model* m_model;
	const class StaticPropMaterial* m_material;
};


