#include "CubeSpawner.hpp"
#include "ECWallMounted.hpp"
#include "ECActivatable.hpp"
#include "ECEditorVisible.hpp"
#include "Cube.hpp"
#include "../WorldUpdateArgs.hpp"
#include "../World.hpp"
#include "../../../Protobuf/Build/CubeSpawnerEntity.pb.h"

#include <imgui.h>

namespace CubeSpawner
{
	struct ECCubeSpawner
	{
		void HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message);
		
		static eg::MessageReceiver MessageReceiver;
		
		eg::EntityHandle cube;
		bool wasActivated = false;
		bool cubeCanFloat = false;
	};
	
	eg::MessageReceiver ECCubeSpawner::MessageReceiver = eg::MessageReceiver::Create<
	    ECCubeSpawner, EditorRenderImGuiMessage>();
	
	eg::EntitySignature EntitySignature = eg::EntitySignature::Create<
		ECWallMounted, ECActivatable, ECEditorVisible, eg::ECPosition3D, ECCubeSpawner>();
	
	void ECCubeSpawner::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
	{
		ECEditorVisible::RenderDefaultSettings(entity);
		
		ImGui::Checkbox("Float", &cubeCanFloat);
	}
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager)
	{
		eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
		
		entity.InitComponent<ECEditorVisible>("Cube Spawner", 4);
		
		return &entity;
	}
	
	void Update(const WorldUpdateArgs& updateArgs)
	{
		for (eg::Entity& entity : updateArgs.world->EntityManager().GetEntitySet(EntitySignature))
		{
			bool activated = entity.GetComponent<ECActivatable>().AllSourcesActive();
			ECCubeSpawner& cubeSpawner = entity.GetComponent<ECCubeSpawner>();
			
			bool spawnNew = false;
			if (cubeSpawner.cube.Get() == nullptr)
			{
				spawnNew = true;
			}
			else if (activated && !cubeSpawner.wasActivated)
			{
				cubeSpawner.cube.Get()->Despawn();
				spawnNew = true;
			}
			
			if (spawnNew)
			{
				glm::vec3 spawnDir(DirectionVector(entity.GetComponent<ECWallMounted>().wallUp));
				glm::vec3 spawnPos = entity.GetComponent<eg::ECPosition3D>().position + spawnDir * (Cube::RADIUS + 0.01f);
				eg::Entity& cube = *Cube::Spawn(updateArgs.world->EntityManager(), spawnPos, cubeSpawner.cubeCanFloat);
				updateArgs.world->InitRigidBodyEntity(cube);
				cubeSpawner.cube = cube;
			}
			
			cubeSpawner.wasActivated = activated;
		}
	}
	
	struct Serializer : eg::IEntitySerializer
	{
		std::string_view GetName() const override
		{
			return "CubeSpawner";
		}
		
		void Serialize(const eg::Entity& entity, std::ostream& stream) const override
		{
			gravity_pb::CubeSpawnerEntity cbSpawnerPB;
			
			glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
			cbSpawnerPB.set_posx(pos.x);
			cbSpawnerPB.set_posy(pos.y);
			cbSpawnerPB.set_posz(pos.z);
			
			cbSpawnerPB.set_dir((gravity_pb::Dir)entity.GetComponent<ECWallMounted>().wallUp);
			
			cbSpawnerPB.set_cube_can_float(entity.GetComponent<ECCubeSpawner>().cubeCanFloat);
			
			const ECActivatable& activatable = entity.GetComponent<ECActivatable>();
			cbSpawnerPB.set_name(activatable.Name());
			
			cbSpawnerPB.SerializeToOstream(&stream);
		}
		
		void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
		{
			eg::Entity& entity = *CreateEntity(entityManager);
			
			gravity_pb::CubeSpawnerEntity cbSpawnerPB;
			cbSpawnerPB.ParseFromIstream(&stream);
			
			entity.InitComponent<eg::ECPosition3D>(cbSpawnerPB.posx(), cbSpawnerPB.posy(), cbSpawnerPB.posz());
			entity.GetComponent<ECWallMounted>().wallUp = (Dir)cbSpawnerPB.dir();
			entity.GetComponent<ECCubeSpawner>().cubeCanFloat = cbSpawnerPB.cube_can_float();
			
			ECActivatable& activatable = entity.GetComponent<ECActivatable>();
			if (cbSpawnerPB.name() != 0)
				activatable.SetName(cbSpawnerPB.name());
		}
	};
	
	eg::IEntitySerializer* EntitySerializer = new Serializer;
}
