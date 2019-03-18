#include "PointLightEntity.hpp"

void PointLightEntity::InitDrawData(PointLightDrawData& data) const
{
	data.position = Position();
	data.radiance = Radiance();
	data.range = Range();
}

void PointLightEntity::EditorRenderSettings()
{
	LightSourceEntity::EditorRenderSettings();
}

void PointLightEntity::Save(YAML::Emitter& emitter) const
{
	LightSourceEntity::Save(emitter);
}

void PointLightEntity::Load(const YAML::Node& node)
{
	LightSourceEntity::Load(node);
}

int PointLightEntity::GetEditorIconIndex() const
{
	return 2;
}
