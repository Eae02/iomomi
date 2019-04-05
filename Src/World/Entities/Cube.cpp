#include "Cube.hpp"
#include "ECEditorVisible.hpp"
#include "ECRigidBody.hpp"
#include "ECFloorButton.hpp"
#include "ECActivator.hpp"
#include "ECInteractable.hpp"
#include "Messages.hpp"
#include "../Clipping.hpp"
#include "../Player.hpp"
#include "../WorldUpdateArgs.hpp"
#include "../World.hpp"
#include "../../Graphics/RenderSettings.hpp"
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
		
		bool isPickedUp = false;
	};
	
	eg::MessageReceiver ECCube::MessageReceiver = eg::MessageReceiver::Create<
	    ECCube, EditorSpawnedMessage, EditorDrawMessage, EditorRenderImGuiMessage, DrawMessage>();
	
	eg::EntitySignature EntitySignature = eg::EntitySignature::Create<
	    ECCube, ECEditorVisible, ECRigidBody, ECInteractable, eg::ECPosition3D, eg::ECRotation3D>();
	
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
	
	inline eg::Sphere GetSphere(const eg::Entity& entity)
	{
		return eg::Sphere(entity.GetComponent<eg::ECPosition3D>().position, RADIUS * std::sqrt(3.0f));
	}
	
	static void Interact(eg::Entity& entity, Player& player)
	{
		ECCube& cube = entity.GetComponent<ECCube>();
		cube.isPickedUp = !cube.isPickedUp;
		
		if (cube.isPickedUp)
		{
			entity.GetComponent<eg::ECPosition3D>().position += glm::vec3(0, 0.1f, 0);
			ECRigidBody::PullTransform(entity);
		}
	}
	
	static int CheckInteraction(const eg::Entity& entity, const Player& player)
	{
		static constexpr int PICK_UP_INTERACT_PRIORITY = 2;
		
		//Very high so that dropping the cube has higher priority than other interactions.
		static constexpr int DROP_INTERACT_PRIORITY = 1000;
		
		if (entity.GetComponent<ECCube>().isPickedUp)
			return DROP_INTERACT_PRIORITY;
		
		static constexpr float MAX_INTERACT_DIST = 1.5f;
		eg::Ray ray(player.Position(), player.Forward());
		float intersectDist;
		if (ray.Intersects(GetSphere(entity), intersectDist) &&
		    intersectDist > 0.0f && intersectDist < MAX_INTERACT_DIST)
		{
			return PICK_UP_INTERACT_PRIORITY;
		}
		
		return 0;
	}
	
	void Update(const WorldUpdateArgs& args)
	{
		if (args.player)
		{
			for (eg::Entity& entity : args.world->EntityManager().GetEntitySet(EntitySignature))
			{
				auto GetPosition = [&] { return entity.GetComponent<eg::ECPosition3D>().position; };
				
				ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
				ECRigidBody::PushTransform(entity);
				
				if (entity.GetComponent<ECCube>().isPickedUp)
				{
					constexpr float DIST_FROM_PLAYER = RADIUS + 0.5f;
					glm::vec3 desiredPosition = args.player->Position() + args.player->Forward() * DIST_FROM_PLAYER;
					
					eg::AABB desiredAABB(desiredPosition - RADIUS * 1.001f, desiredPosition + RADIUS * 1.001f);
					desiredPosition += CalcWorldCollisionCorrection(*args.world, desiredAABB);
					
					glm::vec3 deltaPos = desiredPosition - GetPosition();
					
					rigidBody.GetRigidBody()->setGravity(btVector3(0, 0, 0));
					rigidBody.GetRigidBody()->setLinearVelocity(btVector3(0, 0, 0));
					rigidBody.GetRigidBody()->setAngularVelocity(btVector3(0, 0, 0));
					rigidBody.GetRigidBody()->clearForces();
					rigidBody.GetRigidBody()->applyCentralImpulse(bullet::FromGLM(deltaPos / args.dt));
				}
				else
				{
					rigidBody.GetRigidBody()->setGravity(btVector3(0, -bullet::GRAVITY, 0));
				}
				
				if (rigidBody.GetRigidBody()->getLinearVelocity().length2() > 1E-4f ||
				    rigidBody.GetRigidBody()->getAngularVelocity().length2() > 1E-4f)
				{
					args.invalidateShadows(GetSphere(entity));
				}
				
				const glm::vec3 down(0, -1, 0);
				const eg::AABB cubeAABB(GetPosition() - RADIUS, GetPosition() + RADIUS);
				
				for (eg::Entity& buttonEntity : args.world->EntityManager().GetEntitySet(ECFloorButton::EntitySignature))
				{
					glm::vec3 toButton = glm::normalize(eg::GetEntityPosition(buttonEntity) - GetPosition());
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
		
		entity.InitComponent<ECInteractable>("Pick Up", &Interact, &CheckInteraction);
		
		entity.InitComponent<ECEditorVisible>("Cube");
		
		ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
		rigidBody.Init(MASS, *collisionShape);
		rigidBody.GetRigidBody()->setFlags(BT_DISABLE_WORLD_GRAVITY);
		
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
