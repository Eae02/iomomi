#include "GravitySwitch.hpp"
#include "ECDrawable.hpp"
#include "ECEditorVisible.hpp"
#include "ECWallMounted.hpp"
#include "../Voxel.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../YAMLUtils.hpp"

namespace GravitySwitch
{
	eg::EntitySignature EntitySignature = eg::EntitySignature::Create<
		ECDrawable, ECEditorVisible, ECWallMounted, eg::ECPosition3D>();
	
	static constexpr float SCALE = 1.0f;
	
	static eg::Model* s_model;
	static eg::IMaterial* s_material;
	
	static void OnInit()
	{
		s_model = &eg::GetAsset<eg::Model>("Models/GravitySwitch.obj");
		s_material = &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
	}
	
	EG_ON_INIT(OnInit)
	
	void Draw(eg::Entity& entity, eg::MeshBatch& meshBatch)
	{
		meshBatch.Add(*s_model, *s_material, ECWallMounted::GetTransform(entity, SCALE));
	}
	
	void EditorDraw(eg::Entity& entity, bool selected, const EditorDrawArgs& drawArgs)
	{
		drawArgs.meshBatch->Add(*s_model, *s_material, ECWallMounted::GetTransform(entity, SCALE));
	}
	
	eg::AABB GetAABB(const eg::Entity& entity)
	{
		return ECWallMounted::GetAABB(entity, SCALE, 0.2f);
	}
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager)
	{
		eg::Entity& entity = entityManager.AddEntity(EntitySignature);
		
		entity.GetComponent<ECDrawable>()->SetCallback(&Draw);
		entity.GetComponent<ECEditorVisible>()->Init("Gravity Switch", &EditorDraw, &ECEditorVisible::RenderDefaultSettings);
		
		return &entity;
	}
	
	void InitFromYAML(eg::Entity& entity, const YAML::Node& node)
	{
		entity.GetComponent<eg::ECPosition3D>()->position = ReadYAMLVec3(node["pos"]);
		entity.GetComponent<ECWallMounted>()->wallUp = (Dir)node["up"].as<int>();
	}
}
