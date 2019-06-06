#include "Cube.hpp"
#include "ECEditorVisible.hpp"
#include "ECRigidBody.hpp"
#include "ECFloorButton.hpp"
#include "ECActivator.hpp"
#include "ECInteractable.hpp"
#include "Messages.hpp"
#include "../GravityGun.hpp"
#include "../Clipping.hpp"
#include "../Player.hpp"
#include "../WorldUpdateArgs.hpp"
#include "../World.hpp"
#include "../../Graphics/RenderSettings.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Protobuf/Build/CubeEntity.pb.h"

namespace Cube
{
	static constexpr float MASS = 1.0f;
	static constexpr float RADIUS = 0.4f;
	
	static std::unique_ptr<btCollisionShape> collisionShape;
	
	static eg::Model* cubeModel;
	static eg::IMaterial* cubeMaterial;
	
	static void OnInit()
	{
		collisionShape = std::make_unique<btBoxShape>(btVector3(RADIUS, RADIUS, RADIUS));
		
		cubeModel = &eg::GetAsset<eg::Model>("Models/Cube.obj");
		cubeMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Cube.yaml");
	}
	
	EG_ON_INIT(OnInit)
	
	struct ECCube
	{
		void HandleMessage(eg::Entity& entity, const EditorSpawnedMessage& message);
		void HandleMessage(eg::Entity& entity, const EditorDrawMessage& message);
		void HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message);
		void HandleMessage(eg::Entity& entity, const DrawMessage& message);
		void HandleMessage(eg::Entity& entity, const RayIntersectMessage& message);
		
		void HandleMessage(eg::Entity& entity, const GravityChargeSetMessage& message);
		void HandleMessage(eg::Entity& entity, const GravityChargeResetMessage& message);
		
		static eg::MessageReceiver MessageReceiver;
		
		bool isPickedUp = false;
		Dir currentDown = Dir::NegY;
	};
	
	eg::MessageReceiver ECCube::MessageReceiver = eg::MessageReceiver::Create<
	    ECCube, EditorSpawnedMessage, EditorDrawMessage, EditorRenderImGuiMessage, DrawMessage,
	    RayIntersectMessage, GravityChargeSetMessage, GravityChargeResetMessage>();
	
	eg::EntitySignature EntitySignature = eg::EntitySignature::Create<
	    ECCube, ECEditorVisible, ECRigidBody, ECInteractable, eg::ECPosition3D, eg::ECRotation3D>();
	
	inline eg::Sphere GetSphere(const eg::Entity& entity)
	{
		return eg::Sphere(entity.GetComponent<eg::ECPosition3D>().position, RADIUS * std::sqrt(3.0f));
	}
	
	void ECCube::HandleMessage(eg::Entity& entity, const EditorSpawnedMessage& message)
	{
		entity.GetComponent<eg::ECPosition3D>().position =
			message.wallPosition + glm::vec3(DirectionVector(message.wallNormal)) * (RADIUS + 0.01f);
	}
	
	void ECCube::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
	{
		glm::mat4 worldMatrix = eg::GetEntityTransform3D(entity) * glm::scale(glm::mat4(1), glm::vec3(RADIUS));
		message.meshBatch->AddModel(*cubeModel, *cubeMaterial, worldMatrix);
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
		
		message.meshBatch->AddModel(*cubeModel, *cubeMaterial, worldMatrix);
	}
	
	void ECCube::HandleMessage(eg::Entity& entity, const RayIntersectMessage& message)
	{
		float distance;
		if (message.rayIntersectArgs->ray.Intersects(GetSphere(entity), distance) &&
		    distance > 0 && distance < message.rayIntersectArgs->distance)
		{
			message.rayIntersectArgs->distance = distance;
			message.rayIntersectArgs->entity = &entity;
		}
	}
	
	void ECCube::HandleMessage(eg::Entity& entity, const GravityChargeSetMessage& message)
	{
		if (isPickedUp)
			return;
		
		currentDown = message.newDown;
		*message.set = true;
	}
	
	void ECCube::HandleMessage(eg::Entity& entity, const GravityChargeResetMessage& message)
	{
		currentDown = Dir::NegY;
	}
	
	static void Interact(eg::Entity& entity, Player& player)
	{
		ECCube& cube = entity.GetComponent<ECCube>();
		cube.isPickedUp = !cube.isPickedUp;
		player.SetIsCarrying(cube.isPickedUp);
		
		if (!cube.isPickedUp)
		{
			ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
			
			//Clear the cube's velocity so that it's impossible to fling the cube around.
			rigidBody.GetRigidBody()->setLinearVelocity(btVector3(0, 0, 0));
			rigidBody.GetRigidBody()->setAngularVelocity(btVector3(0, 0, 0));
		}
	}
	
	static int CheckInteraction(const eg::Entity& entity, const Player& player)
	{
		//Very high so that dropping the cube has higher priority than other interactions.
		static constexpr int DROP_INTERACT_PRIORITY = 1000;
		if (entity.GetComponent<ECCube>().isPickedUp)
			return DROP_INTERACT_PRIORITY;
		
		if (player.IsCarrying() || !player.OnGround())
			return 0;
		
		static constexpr int PICK_UP_INTERACT_PRIORITY = 2;
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
	
	void UpdatePreSim(const WorldUpdateArgs& args)
	{
		for (eg::Entity& entity : args.world->EntityManager().GetEntitySet(EntitySignature))
		{
			ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
			ECCube& cube = entity.GetComponent<ECCube>();
			
			if (cube.isPickedUp)
			{
				ECRigidBody::PushTransform(entity);
				
				constexpr float DIST_FROM_PLAYER = RADIUS + 0.7f; //Distance to the cube when carrying
				constexpr float MAX_DIST_FROM_PLAYER = RADIUS + 0.9f; //Drop the cube if further away than this
				
				glm::vec3 desiredPosition = args.player->EyePosition() + args.player->Forward() * DIST_FROM_PLAYER;
				
				eg::AABB desiredAABB(desiredPosition - RADIUS * 1.001f, desiredPosition + RADIUS * 1.001f);
				
				glm::vec3 deltaPos = desiredPosition - entity.GetComponent<eg::ECPosition3D>().position;
				
				if (glm::length2(deltaPos) > MAX_DIST_FROM_PLAYER * MAX_DIST_FROM_PLAYER)
				{
					cube.isPickedUp = false;
					args.player->SetIsCarrying(false);
				}
				else
				{
					ClippingArgs clipArgs;
					clipArgs.ellipsoid.center = entity.GetComponent<eg::ECPosition3D>().position;
					clipArgs.ellipsoid.radii = glm::vec3(RADIUS * 0.75f);
					clipArgs.move = deltaPos;
					args.world->CalcClipping(clipArgs, cube.currentDown);
					
					if (clipArgs.collisionInfo.collisionFound)
						deltaPos *= clipArgs.collisionInfo.distance;
					
					rigidBody.GetRigidBody()->setGravity(btVector3(0, 0, 0));
					rigidBody.GetRigidBody()->setLinearVelocity(
						bullet::FromGLM(deltaPos / std::max(args.dt, 1.0f / 60.0f)));
					rigidBody.GetRigidBody()->setAngularVelocity(btVector3(0, 0, 0));
					rigidBody.GetRigidBody()->clearForces();
				}
			}
			else
			{
				glm::vec3 gravity = glm::vec3(DirectionVector(cube.currentDown)) * bullet::GRAVITY;
				rigidBody.GetRigidBody()->setGravity(bullet::FromGLM(gravity));
				
				btBroadphaseProxy* bpProxy = rigidBody.GetRigidBody()->getBroadphaseProxy();
				bpProxy->m_collisionFilterGroup = 1 | (2 << ((int)cube.currentDown / 2));
			}
		}
	}
	
	void UpdatePostSim(const WorldUpdateArgs& args)
	{
		for (eg::Entity& entity : args.world->EntityManager().GetEntitySet(EntitySignature))
		{
			glm::vec3 position = entity.GetComponent<eg::ECPosition3D>().position;
			
			ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
			ECRigidBody::PushTransform(entity);
			
			if (rigidBody.GetRigidBody()->getLinearVelocity().length2() > 1E-4f ||
			    rigidBody.GetRigidBody()->getAngularVelocity().length2() > 1E-4f)
			{
				args.invalidateShadows(GetSphere(entity));
			}
			
			const glm::vec3 down(DirectionVector(entity.GetComponent<ECCube>().currentDown));
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
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager)
	{
		eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
		
		entity.InitComponent<ECInteractable>("Pick Up", &Interact, &CheckInteraction);
		
		entity.InitComponent<ECEditorVisible>("Cube", 4);
		
		ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
		rigidBody.Init(MASS, *collisionShape);
		rigidBody.GetRigidBody()->setFlags(rigidBody.GetRigidBody()->getFlags() | BT_DISABLE_WORLD_GRAVITY);
		rigidBody.GetRigidBody()->setActivationState(DISABLE_DEACTIVATION);
		
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
