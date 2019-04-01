#include "GravitySwitch.hpp"
#include "ECDrawable.hpp"
#include "ECEditorVisible.hpp"
#include "ECWallMounted.hpp"
#include "../Voxel.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../YAMLUtils.hpp"
#include "../../../Protobuf/Build/GravitySwitchEntity.pb.h"

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
		eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
		
		entity.GetComponent<ECDrawable>().SetCallback(&Draw);
		entity.InitComponent<ECEditorVisible>("Gravity Switch", &EditorDraw, &ECEditorVisible::RenderDefaultSettings);
		
		return &entity;
	}
	
	struct GravitySwitchSerializer : public eg::IEntitySerializer
	{
		std::string_view GetName() const override
		{
			return "GravitySwitch";
		}
		
		void Serialize(const eg::Entity& entity, std::ostream& stream) const override
		{
			gravity_pb::GravitySwitchEntity switchPB;
			
			switchPB.set_dir((gravity_pb::Dir)entity.GetComponent<ECWallMounted>().wallUp);
			
			glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
			switchPB.set_posx(pos.x);
			switchPB.set_posy(pos.y);
			switchPB.set_posz(pos.z);
			
			switchPB.SerializeToOstream(&stream);
		}
		
		void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
		{
			eg::Entity& entity = *CreateEntity(entityManager);
			
			gravity_pb::GravitySwitchEntity switchPB;
			switchPB.ParseFromIstream(&stream);
			
			entity.InitComponent<eg::ECPosition3D>(switchPB.posx(), switchPB.posy(), switchPB.posz());
			
			entity.GetComponent<ECWallMounted>().wallUp = (Dir)switchPB.dir();
		}
	};
	
	eg::IEntitySerializer* EntitySerializer = new GravitySwitchSerializer;
}
