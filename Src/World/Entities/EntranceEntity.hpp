#pragma once

#include "Entity.hpp"

class EntranceEntity : public Entity, public Entity::IDrawable, public Entity::IEditorWallDrag,
	public Entity::IUpdatable, public Entity::ICollidable
{
public:
	enum class Type
	{
		Entrance,
		Exit
	};
	
	EntranceEntity();
	
	void Draw(class ObjectRenderer& renderer) override;
	
	int GetEditorIconIndex() const override;
	
	void EditorRenderSettings() override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir) override;
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
	void Update(const UpdateArgs& args) override;
	
	void CalcClipping(ClippingArgs& args) const override;
	
private:
	glm::mat4 GetTransform() const;
	
	Dir m_direction = Dir::NegX;
	const eg::Model* m_model;
	std::vector<const class ObjectMaterial*> m_materials;
	
	Type m_type = Type::Exit;
	float m_doorOpenProgress = 0;
	
	size_t m_doorMeshIndices[2][2];
};
