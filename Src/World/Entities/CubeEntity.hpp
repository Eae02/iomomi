#pragma once

#include "Entity.hpp"
#include "IRigidBodyEntity.hpp"

class CubeEntity : public Entity, public Entity::IUpdatable, public Entity::IDrawable, public IRigidBodyEntity
{
public:
	CubeEntity();
	
	void Update(const UpdateArgs& args) override;
	
	void Draw(eg::MeshBatch& meshBatch) override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal) override;
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
	btRigidBody* GetRigidBody() override;
	
private:
	btDefaultMotionState m_motionState;
	btRigidBody m_rigidBody;
};
