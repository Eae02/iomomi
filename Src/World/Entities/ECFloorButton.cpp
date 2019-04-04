#include "ECFloorButton.hpp"
#include "ECWallMounted.hpp"
#include "ECEditorVisible.hpp"
#include "ECActivator.hpp"
#include "Messages.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Protobuf/Build/FloorButtonEntity.pb.h"

static eg::Model* s_model;
static eg::IMaterial* s_material;

static void OnInit()
{
	s_model = &eg::GetAsset<eg::Model>("Models/Button.obj");
	s_material = &eg::GetAsset<StaticPropMaterial>("Materials/Button.yaml");
}

EG_ON_INIT(OnInit)

eg::MessageReceiver ECFloorButton::MessageReceiver = eg::MessageReceiver::Create<
		ECFloorButton, EditorDrawMessage, EditorRenderImGuiMessage, DrawMessage>();

eg::EntitySignature ECFloorButton::EntitySignature = eg::EntitySignature::Create<
    eg::ECPosition3D, ECWallMounted, ECEditorVisible, ECFloorButton, ECActivator>();

void ECFloorButton::HandleMessage(eg::Entity& entity, const DrawMessage& message)
{
	message.meshBatch->Add(*s_model, *s_material, ECWallMounted::GetTransform(entity, SCALE));
}

void ECFloorButton::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
{
	message.meshBatch->Add(*s_model, *s_material, ECWallMounted::GetTransform(entity, SCALE));
}

void ECFloorButton::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
{
	ECEditorVisible::RenderDefaultSettings(entity);
}

eg::Entity* ECFloorButton::CreateEntity(eg::EntityManager& entityManager)
{
	eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
	
	entity.InitComponent<ECEditorVisible>("Floor Button");
	
	return &entity;
}

struct FloorButtonSerializer : public eg::IEntitySerializer
{
	std::string_view GetName() const override
	{
		return "FloorButton";
	}
	
	void Serialize(const eg::Entity& entity, std::ostream& stream) const override
	{
		gravity_pb::FloorButtonEntity buttonPB;
		
		buttonPB.set_dir((gravity_pb::Dir)entity.GetComponent<ECWallMounted>().wallUp);
		
		glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
		buttonPB.set_posx(pos.x);
		buttonPB.set_posy(pos.y);
		buttonPB.set_posz(pos.z);
		
		const ECActivator& activator = entity.GetComponent<ECActivator>();
		buttonPB.set_targetname(activator.activatableName);
		buttonPB.set_sourceindex(activator.sourceIndex);
		
		buttonPB.SerializeToOstream(&stream);
	}
	
	void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
	{
		eg::Entity& entity = *ECFloorButton::CreateEntity(entityManager);
		
		gravity_pb::FloorButtonEntity buttonPB;
		buttonPB.ParseFromIstream(&stream);
		
		entity.InitComponent<eg::ECPosition3D>(buttonPB.posx(), buttonPB.posy(), buttonPB.posz());
		
		entity.GetComponent<ECWallMounted>().wallUp = (Dir)buttonPB.dir();
		
		ECActivator& activator = entity.GetComponent<ECActivator>();
		activator.sourceIndex = buttonPB.sourceindex();
		activator.activatableName = buttonPB.targetname();
	}
};

eg::IEntitySerializer* ECFloorButton::EntitySerializer = new FloorButtonSerializer;
