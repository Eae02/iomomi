#include "GravitySwitch.hpp"
#include "ECEditorVisible.hpp"
#include "ECWallMounted.hpp"
#include "Messages.hpp"
#include "../Voxel.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../YAMLUtils.hpp"
#include "../../../Protobuf/Build/GravitySwitchEntity.pb.h"

namespace GravitySwitch
{
	struct ECGravitySwitch
	{
		void HandleMessage(eg::Entity& entity, const EditorDrawMessage& message);
		void HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message);
		void HandleMessage(eg::Entity& entity, const DrawMessage& message);
		
		static eg::MessageReceiver MessageReceiver;
	};
	
	eg::MessageReceiver ECGravitySwitch::MessageReceiver = eg::MessageReceiver::Create<
		ECGravitySwitch, EditorDrawMessage, EditorRenderImGuiMessage, DrawMessage>();
	
	eg::EntitySignature EntitySignature = eg::EntitySignature::Create<
		ECEditorVisible, ECWallMounted, ECGravitySwitch, eg::ECPosition3D>();
	
	static eg::Model* s_model;
	static eg::IMaterial* s_material;
	
	static void OnInit()
	{
		s_model = &eg::GetAsset<eg::Model>("Models/GravitySwitch.obj");
		s_material = &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
	}
	
	EG_ON_INIT(OnInit)
	
	void ECGravitySwitch::HandleMessage(eg::Entity& entity, const DrawMessage& message)
	{
		message.meshBatch->Add(*s_model, *s_material, ECWallMounted::GetTransform(entity, SCALE));
	}
	
	void ECGravitySwitch::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
	{
		ECEditorVisible::RenderDefaultSettings(entity);
	}
	
	void ECGravitySwitch::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
	{
		message.meshBatch->Add(*s_model, *s_material, ECWallMounted::GetTransform(entity, SCALE));
	}
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager)
	{
		eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
		
		entity.InitComponent<ECEditorVisible>("Gravity Switch");
		
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
