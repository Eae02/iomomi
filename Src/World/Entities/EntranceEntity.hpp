#pragma once

#include "Entity.hpp"

class EntranceEntity : public Entity, public Entity::IDrawable, public Entity::IEditorWallDrag,
	public Entity::IUpdatable, public Entity::ICollidable
{
public:
	enum class Type
	{
		Entrance = 0,
		Exit = 1
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
	
	Type EntranceType() const
	{
		return m_type;
	}
	
	void InitPlayer(class Player& player) const;
	
private:
	glm::mat4 GetTransform() const;
	
	Dir m_direction = Dir::NegX;
	
	Type m_type = Type::Exit;
	float m_doorOpenProgress = 0;
};
