#pragma once

#include "Entity.hpp"
#include "IPointLightEntity.hpp"
#include "../../Graphics/Materials/EmissiveMaterial.hpp"

class WallLightEntity : public Entity, public IPointLightEntity, public Entity::IDrawable, public Entity::IEditorWallDrag
{
public:
	void Draw(eg::MeshBatch& meshBatch) override;
	
	void EditorDraw(bool selected, const EditorDrawArgs& drawArgs) const override;
	
	void EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir) override;
	
	void EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal) override;
	
	void GetPointLights(std::vector<PointLightDrawData>& drawData) const override;
	
	int GetEditorIconIndex() const override;
	
	void EditorRenderSettings() override;
	
	void Save(YAML::Emitter& emitter) const override;
	
	void Load(const YAML::Node& node) override;
	
private:
	glm::mat4 GetTransform() const;
	
	EmissiveMaterial::InstanceData GetInstanceData(float colorScale) const;
	
	PointLight m_pointLight;
	
	Dir m_dir;
};
