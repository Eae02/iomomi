#include "WallLight.hpp"
#include "ECEditorVisible.hpp"
#include "ECWallMounted.hpp"
#include "Messages.hpp"
#include "../../Graphics/Lighting/PointLight.hpp"
#include "../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../YAMLUtils.hpp"
#include "../../Graphics/Lighting/Serialize.hpp"
#include "../../../Protobuf/Build/WallLightEntity.pb.h"

#include <imgui.h>

namespace WallLight
{
	struct ECWallLight
	{
		void HandleMessage(eg::Entity& entity, const EditorDrawMessage& message);
		void HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message);
		void HandleMessage(eg::Entity& entity, const DrawMessage& message);
		
		static eg::MessageReceiver MessageReceiver;
	};
	
	eg::MessageReceiver ECWallLight::MessageReceiver = eg::MessageReceiver::Create<
		ECWallLight, EditorDrawMessage, EditorRenderImGuiMessage, DrawMessage>();
	
	static eg::EntitySignature entitySignature = eg::EntitySignature::Create<
		ECEditorVisible, ECWallMounted, ECWallLight, eg::ECPosition3D>();
	
	static eg::EntitySignature lightChildSignature = eg::EntitySignature::Create<eg::ECPosition3D, PointLight>();
	
	static eg::Model* model;
	
	static constexpr float LIGHT_DIST = 0.5f;
	static constexpr float MODEL_SCALE = 0.25f;
	
	static void OnInit()
	{
		model = &eg::GetAsset<eg::Model>("Models/WallLight.obj");
	}
	
	EG_ON_INIT(OnInit)
	
	template <typename E>
	inline auto& GetPointLight(E& entity)
	{
		return eg::Deref(entity.FindChildBySignature(lightChildSignature)).template GetComponent<PointLight>();
	}
	
	inline EmissiveMaterial::InstanceData GetInstanceData(const eg::Entity& entity, float colorScale)
	{
		eg::ColorLin color = eg::ColorLin(GetPointLight(entity).GetColor()).ScaleRGB(colorScale);
		
		EmissiveMaterial::InstanceData instanceData;
		instanceData.transform = ECWallMounted::GetTransform(entity, MODEL_SCALE);
		instanceData.color = glm::vec4(color.r, color.g, color.b, 1.0f);
		
		return instanceData;
	}
	
	void ECWallLight::HandleMessage(eg::Entity& entity, const DrawMessage& message)
	{
		message.meshBatch->AddModel(*model, EmissiveMaterial::instance, GetInstanceData(entity, 4.0f));
	}
	
	void ECWallLight::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
	{
		message.meshBatch->AddModel(*model, EmissiveMaterial::instance, GetInstanceData(entity, 1.0f));
	}
	
	void ECWallLight::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
	{
		ECEditorVisible::RenderDefaultSettings(entity);
		
		ImGui::Separator();
		
		GetPointLight(entity).RenderRadianceSettings();
	}
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager)
	{
		eg::Entity& entity = entityManager.AddEntity(entitySignature, nullptr, EntitySerializer);
		
		entity.InitComponent<ECEditorVisible>("Wall Light", 3);
		
		entityManager.AddEntity(lightChildSignature, &entity);
		
		return &entity;
	}
	
	struct WallLightSerializer : public eg::IEntitySerializer
	{
		std::string_view GetName() const override
		{
			return "WallLight";
		}
		
		void Serialize(const eg::Entity& entity, std::ostream& stream) const override
		{
			gravity_pb::WallLightEntity wallLightPB;
			
			wallLightPB.set_dir((gravity_pb::Dir)entity.GetComponent<ECWallMounted>().wallUp);
			
			glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
			wallLightPB.set_posx(pos.x);
			wallLightPB.set_posy(pos.y);
			wallLightPB.set_posz(pos.z);
			
			const PointLight& light = GetPointLight(entity);
			wallLightPB.set_colorr(light.GetColor().r);
			wallLightPB.set_colorg(light.GetColor().g);
			wallLightPB.set_colorb(light.GetColor().b);
			wallLightPB.set_intensity(light.Intensity());
			
			wallLightPB.SerializeToOstream(&stream);
		}
		
		void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
		{
			eg::Entity& entity = *CreateEntity(entityManager);
			
			gravity_pb::WallLightEntity wallLightPB;
			wallLightPB.ParseFromIstream(&stream);
			
			entity.InitComponent<eg::ECPosition3D>(wallLightPB.posx(), wallLightPB.posy(), wallLightPB.posz());
			
			const Dir dir = (Dir)wallLightPB.dir();
			entity.GetComponent<ECWallMounted>().wallUp = dir;
			
			eg::Entity& lightEntity = eg::Deref(entity.FindChildBySignature(lightChildSignature));
			
			lightEntity.InitComponent<eg::ECPosition3D>(glm::vec3(DirectionVector(dir)) * LIGHT_DIST);
			
			eg::ColorSRGB color(wallLightPB.colorr(), wallLightPB.colorg(), wallLightPB.colorb());
			lightEntity.GetComponent<PointLight>().SetRadiance(color, wallLightPB.intensity());
		}
	};
	
	eg::IEntitySerializer* EntitySerializer = new WallLightSerializer;
}
