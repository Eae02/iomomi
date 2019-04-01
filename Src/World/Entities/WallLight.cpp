#include "WallLight.hpp"
#include "ECDrawable.hpp"
#include "ECEditorVisible.hpp"
#include "ECWallMounted.hpp"
#include "../../Graphics/Lighting/PointLight.hpp"
#include "../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../YAMLUtils.hpp"
#include "../../Graphics/Lighting/Serialize.hpp"

#include <imgui.h>

namespace WallLight
{
	static eg::EntitySignature entitySignature = eg::EntitySignature::Create<
		ECDrawable, ECEditorVisible, ECWallMounted, eg::ECPosition3D, PointLight>();
	
	static eg::Model* model;
	
	static constexpr float LIGHT_DIST = 0.5f;
	static constexpr float MODEL_SCALE = 0.25f;
	
	static void OnInit()
	{
		model = &eg::GetAsset<eg::Model>("Models/WallLight.obj");
	}
	
	EG_ON_INIT(OnInit)
	
	inline EmissiveMaterial::InstanceData GetInstanceData(const eg::Entity& entity, float colorScale)
	{
		const PointLight* light = entity.GetComponent<PointLight>();
		
		eg::ColorLin color = eg::ColorLin(light->GetColor()).ScaleRGB(colorScale);
		
		EmissiveMaterial::InstanceData instanceData;
		instanceData.transform = ECWallMounted::GetTransform(entity, MODEL_SCALE);
		instanceData.color = glm::vec4(color.r, color.g, color.b, 1.0f);
		
		return instanceData;
	}
	
	void Draw(eg::Entity& entity, eg::MeshBatch& meshBatch)
	{
		meshBatch.Add(*model, EmissiveMaterial::instance, GetInstanceData(entity, 4.0f));
	}
	
	void EditorDraw(eg::Entity& entity, bool selected, const EditorDrawArgs& drawArgs)
	{
		drawArgs.meshBatch->Add(*model, EmissiveMaterial::instance, GetInstanceData(entity, 1.0f));
	}
	
	void EditorRenderSettings(eg::Entity& entity)
	{
		ECEditorVisible::RenderDefaultSettings(entity);
		
		ImGui::Separator();
		
		entity.GetComponent<PointLight>()->RenderRadianceSettings();
	}
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager)
	{
		eg::Entity& entity = entityManager.AddEntity(entitySignature);
		
		entity.GetComponent<ECDrawable>()->SetCallback(&Draw);
		entity.GetComponent<ECEditorVisible>()->Init("Wall Light", 3, &EditorDraw, &EditorRenderSettings);
		
		return &entity;
	}
	
	void InitFromYAML(eg::Entity& entity, const YAML::Node& node)
	{
		entity.GetComponent<eg::ECPosition3D>()->position = ReadYAMLVec3(node["pos"]);
		entity.GetComponent<ECWallMounted>()->wallUp = (Dir)node["up"].as<int>();
		DeserializePointLight(node, *entity.GetComponent<PointLight>());
	}
}
