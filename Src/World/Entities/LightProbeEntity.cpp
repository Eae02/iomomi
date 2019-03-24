#include "LightProbeEntity.hpp"
#include "../../Graphics/Lighting/LightProbesManager.hpp"
#include "../../Editor/PrimitiveRenderer.hpp"
#include "../../YAMLUtils.hpp"

#include <imgui.h>

LightProbeEntity::LightProbeEntity()
{
	m_influenceRadius = glm::vec3(1.0f);
	m_parallaxRadius = glm::vec3(1.0f);
	m_influenceFade = glm::vec3(0.5f);
}

int LightProbeEntity::GetEditorIconIndex() const
{
	return Entity::GetEditorIconIndex();
}

void LightProbeEntity::EditorRenderSettings()
{
	Entity::EditorRenderSettings();
	
	ImGui::Separator();
	
	if (ImGui::DragFloat3("Parallax Radius", &m_parallaxRadius.x, 0.1f, 0.0f, INFINITY))
	{
		m_parallaxRadius = glm::max(m_parallaxRadius, glm::vec3(0.0f));
	}
	
	if (ImGui::DragFloat3("Influence Radius", &m_influenceRadius.x, 0.1f, 0.0f, INFINITY))
	{
		m_influenceRadius = glm::max(m_influenceRadius, glm::vec3(0.0f));
	}
	
	if (ImGui::DragFloat3("Influence Fade", &m_influenceFade.x, 0.1f, 0.0f, INFINITY))
	{
		m_influenceFade = glm::max(m_influenceFade, glm::vec3(0.0f));
	}
}

void LightProbeEntity::EditorDraw(bool selected, const Entity::EditorDrawArgs& drawArgs) const
{
	if (!selected)
		return;
	
	
}

void LightProbeEntity::Save(YAML::Emitter& emitter) const
{
	Entity::Save(emitter);
	
	WriteYAMLVec3(emitter, "pRad", m_parallaxRadius);
	WriteYAMLVec3(emitter, "iRad", m_influenceRadius);
	WriteYAMLVec3(emitter, "iFade", m_influenceFade);
}

void LightProbeEntity::Load(const YAML::Node& node)
{
	Entity::Load(node);
	
	m_parallaxRadius = ReadYAMLVec3(node["pRad"]);
	m_influenceRadius = ReadYAMLVec3(node["iRad"]);
	m_influenceFade = ReadYAMLVec3(node["iFade"]);
}

void LightProbeEntity::GetParameters(LightProbe& probeOut) const
{
	probeOut.position = Position();
	probeOut.parallaxRad = m_parallaxRadius;
	probeOut.influenceRad = m_influenceRadius;
	probeOut.influenceFade = m_influenceFade;
}
