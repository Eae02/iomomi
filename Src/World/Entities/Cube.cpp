#include "Cube.hpp"
#include "ECEditorVisible.hpp"
#include "ECRigidBody.hpp"
#include "ECFloorButton.hpp"
#include "ECActivator.hpp"
#include "Messages.hpp"
#include "../WorldUpdateArgs.hpp"
#include "../World.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Protobuf/Build/CubeEntity.pb.h"

namespace Cube
{
	static constexpr float MASS = 10.0f;
	static constexpr float RADIUS = 0.4f;
	
	static std::unique_ptr<btCollisionShape> collisionShape;
	
	static eg::Model* cubeModel;
	static eg::IMaterial* cubeMaterial;
	
	static void OnInit()
	{
		collisionShape = std::make_unique<btBoxShape>(btVector3(RADIUS, RADIUS, RADIUS));
		
		cubeModel = &eg::GetAsset<eg::Model>("Models/Cube.obj");
		cubeMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
	}
	
	EG_ON_INIT(OnInit)
	
	struct ECCube
	{
		void HandleMessage(eg::Entity& entity, const EditorSpawnedMessage& message);
		void HandleMessage(eg::Entity& entity, const EditorDrawMessage& message);
		void HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message);
		void HandleMessage(eg::Entity& entity, const DrawMessage& message);
		
		static eg::MessageReceiver MessageReceiver;
	};
	
	eg::MessageReceiver ECCube::MessageReceiver = eg::MessageReceiver::Create<
	    ECCube, EditorSpawnedMessage, EditorDrawMessage, EditorRenderImGuiMessage, DrawMessage>();
	
	eg::EntitySignature EntitySignature = eg::EntitySignature::Create<
	    ECCube, ECEditorVisible, ECRigidBody, eg::ECPosition3D, eg::ECRotation3D>();
	
	void ECCube::HandleMessage(eg::Entity& entity, const EditorSpawnedMessage& message)
	{
		entity.GetComponent<eg::ECPosition3D>().position =
			message.wallPosition + glm::vec3(DirectionVector(message.wallNormal)) * (RADIUS + 0.01f);
	}
	
	void ECCube::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
	{
		glm::mat4 worldMatrix = eg::GetEntityTransform3D(entity) * glm::scale(glm::mat4(1), glm::vec3(RADIUS));
		message.meshBatch->Add(*cubeModel, *cubeMaterial, worldMatrix);
	}
	
	void ECCube::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
	{
		ECEditorVisible::RenderDefaultSettings(entity);
	}
	
	void ECCube::HandleMessage(eg::Entity& entity, const DrawMessage& message)
	{
		btTransform transform = entity.GetComponent<ECRigidBody>().GetBulletTransform();
		
		glm::mat4 worldMatrix;
		transform.getOpenGLMatrix(reinterpret_cast<float*>(&worldMatrix));
		worldMatrix *= glm::scale(glm::mat4(1), glm::vec3(RADIUS));
		
		message.meshBatch->Add(*cubeModel, *cubeMaterial, worldMatrix);
	}
	
	void Update(const WorldUpdateArgs& args)
	{
		if (args.player)
		{
			for (eg::Entity& entity : args.world->EntityManager().GetEntitySet(EntitySignature))
			{
				ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
				if (rigidBody.GetRigidBody()->getLinearVelocity().length2() > 1E-4f ||
				    rigidBody.GetRigidBody()->getAngularVelocity().length2() > 1E-4f)
				{
					ECRigidBody::PushTransform(entity);
					
					args.invalidateShadows(eg::Sphere(eg::GetEntityPosition(entity), RADIUS * std::sqrt(3.0f)));
				}
				
				const glm::vec3 position = entity.GetComponent<eg::ECPosition3D>().position;
				const glm::vec3 down(0, -1, 0);
				const eg::AABB cubeAABB(position - RADIUS, position + RADIUS);
				
				for (eg::Entity& buttonEntity : args.world->EntityManager().GetEntitySet(ECFloorButton::EntitySignature))
				{
					glm::vec3 toButton = glm::normalize(eg::GetEntityPosition(buttonEntity) - position);
					if (glm::dot(toButton, down) > 0.1f && ECFloorButton::GetAABB(buttonEntity).Intersects(cubeAABB))
					{
						buttonEntity.HandleMessage(ActivateMessage());
					}
				}
			}
		}
	}
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager)
	{
		eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
		
		entity.InitComponent<ECEditorVisible>("Cube");
		
		entity.GetComponent<ECRigidBody>().Init(MASS, *collisionShape);
		
		return &entity;
	}
	
	struct Serializer : eg::IEntitySerializer
	{
		std::string_view GetName() const override
		{
			return "Cube";
		}
		
		void Serialize(const eg::Entity& entity, std::ostream& stream) const override
		{
			gravity_pb::CubeEntity cubePB;
			
			glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
			cubePB.set_posx(pos.x);
			cubePB.set_posy(pos.y);
			cubePB.set_posz(pos.z);
			
			glm::quat rotation = entity.GetComponent<eg::ECRotation3D>().rotation;
			cubePB.set_rotationx(rotation.x);
			cubePB.set_rotationy(rotation.y);
			cubePB.set_rotationz(rotation.z);
			cubePB.set_rotationw(rotation.w);
			
			cubePB.SerializeToOstream(&stream);
		}
		
		void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
		{
			eg::Entity& entity = *CreateEntity(entityManager);
			
			gravity_pb::CubeEntity cubePB;
			cubePB.ParseFromIstream(&stream);
			
			entity.InitComponent<eg::ECPosition3D>(cubePB.posx(), cubePB.posy(), cubePB.posz());
			entity.InitComponent<eg::ECRotation3D>(glm::quat(
				cubePB.rotationw(), cubePB.rotationx(), cubePB.rotationy(), cubePB.rotationz()));
			
			ECRigidBody::PullTransform(entity);
		}
	};
	
	eg::IEntitySerializer* EntitySerializer = new Serializer;
}
