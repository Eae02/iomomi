#pragma once

#include "PointLightEntity.hpp"
#include "../../Graphics/Materials/EmissiveMaterial.hpp"

class WallLightEntity : public PointLightEntity, public Entity::IDrawable, public Entity::IEditorWallDrag
{
public:
	void Draw(eg::MeshBatch& meshBatch) override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir) override;
	
	void EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal) override;
	
	void InitDrawData(PointLightDrawData& data) const override;
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
private:
	glm::mat4 GetTransform() const;
	
	Dir m_dir;
	EmissiveMaterial m_material;
};
