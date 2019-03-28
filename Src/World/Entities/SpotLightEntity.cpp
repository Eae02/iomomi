#include "SpotLightEntity.hpp"
#include "../../Editor/PrimitiveRenderer.hpp"
#include "../../Graphics/Lighting/Serialize.hpp"
#include <imgui.h>

void SpotLightEntity::EditorSpawned(const glm::vec3& wallPosition, Dir wallNormal)
{
	SetPosition(wallPosition);
	m_spotLight.SetDirection(DirectionVector(wallNormal));
}

void SpotLightEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
	SerializeSpotLight(emitter, m_spotLight);
}

void SpotLightEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
	DeserializeSpotLight(node, m_spotLight);
}

void SpotLightEntity::EditorRenderSettings()
{
	m_spotLight.RenderRadianceSettings();
	
	ImGui::Separator();
	
	float cutoffAngle = m_spotLight.CutoffAngle();
	float penumbraAngle = m_spotLight.PenumbraAngle();
	bool angleChanged = false;
	
	angleChanged |= ImGui::SliderAngle("Cutoff Angle", &cutoffAngle, 5.0f, 179.0f);
	angleChanged |= ImGui::SliderAngle("Penumbra Angle", &penumbraAngle, 0.0f, glm::degrees(cutoffAngle));
	
	if (angleChanged)
	{
		m_spotLight.SetCutoff(cutoffAngle, penumbraAngle);
	}
}

const glm::vec3 coneVertices[] = 
{
	glm::vec3(1, 1, 0),
	glm::vec3(-1, 1, 0),
	glm::vec3(0, 1, 1),
	glm::vec3(0, 1, -1)
};

void SpotLightEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	if (selected)
	{
		m_spotLight.DrawCone(*drawArgs.primitiveRenderer, Position());
	}
}

void SpotLightEntity::EditorWallDrag(const glm::vec3& newPosition, Dir wallNormalDir)
{
	SetPosition(newPosition);
	m_spotLight.SetDirection(DirectionVector(wallNormalDir));
}

void SpotLightEntity::GetSpotLights(std::vector<SpotLightDrawData>& drawData) const
{
	drawData.push_back(m_spotLight.GetDrawData(Position()));
}
