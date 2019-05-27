#include "ECPlatform.hpp"
#include "ECWallMounted.hpp"
#include "ECEditorVisible.hpp"
#include "ECActivatable.hpp"
#include "ECRigidBody.hpp"
#include "../World.hpp"
#include "../Clipping.hpp"
#include "../WorldUpdateArgs.hpp"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../Protobuf/Build/PlatformEntity.pb.h"

#include <imgui.h>

static eg::Model* platformModel;
static eg::IMaterial* platformMaterial;

static eg::Model* platformSliderModel;
static eg::IMaterial* platformSliderMaterial;

static std::unique_ptr<btCollisionShape> platformCollisionShape;

static void OnInit()
{
	platformModel = &eg::GetAsset<eg::Model>("Models/Platform.obj");
	platformMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
	
	platformSliderModel = &eg::GetAsset<eg::Model>("Models/PlatformSlider.obj");
	platformSliderMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/PlatformSlider.yaml");
	
	platformCollisionShape = std::make_unique<btBoxShape>(btVector3(1.0f, 0.05f, 1.0f));
}

EG_ON_INIT(OnInit)

eg::EntitySignature ECPlatform::EntitySignature = eg::EntitySignature::Create<
    eg::ECPosition3D, ECWallMounted, ECPlatform, ECEditorVisible, ECActivatable, ECRigidBody>();

eg::MessageReceiver ECPlatform::MessageReceiver = eg::MessageReceiver::Create<ECPlatform,
    EditorDrawMessage, EditorRenderImGuiMessage, DrawMessage, RayIntersectMessage, CalculateCollisionMessage>();

std::vector<glm::vec3> GetPlatformConnectionPoints(const eg::Entity& entity);

eg::Entity* ECPlatform::CreateEntity(eg::EntityManager& entityManager)
{
	eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
	
	ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
	rigidBody.InitStatic(*platformCollisionShape);
	rigidBody.GetRigidBody()->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
	rigidBody.GetRigidBody()->setActivationState(DISABLE_DEACTIVATION);
	rigidBody.GetRigidBody()->setFriction(10.0f);
	
	entity.InitComponent<ECEditorVisible>("Platform");
	entity.InitComponent<ECActivatable>(&GetPlatformConnectionPoints);
	
	return &entity;
}

static const glm::mat4 PLATFORM_ROTATION_MATRIX(0, 0, -1, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1);

//Gets the transformation matrix for the platform without any sliding
inline glm::mat4 GetPlatformBaseTransform(const eg::Entity& entity)
{
	return ECWallMounted::GetTransform(entity, 1.0f) * PLATFORM_ROTATION_MATRIX;
}

//Gets the transformation matrix for the platform, slided to it's current position
inline glm::mat4 GetPlatformTransform(const eg::Entity& entity)
{
	const ECPlatform& platform = entity.GetComponent<ECPlatform>();
	const glm::vec2 slideOffset = platform.slideOffset * glm::smoothstep(0.0f, 1.0f, platform.slideProgress);
	
	return glm::translate(GetPlatformBaseTransform(entity), glm::vec3(slideOffset.x, slideOffset.y, 0));
}

std::vector<glm::vec3> GetPlatformConnectionPoints(const eg::Entity& entity)
{
	const glm::mat4 baseTransform = GetPlatformBaseTransform(entity);
	const ECPlatform& platform = entity.GetComponent<ECPlatform>();
	
	const float connectionPoints[] = { 0.0f, 0.5f, 1.0f };
	
	std::vector<glm::vec3> connectionPointsWS(std::size(connectionPoints));
	for (size_t i = 0; i < std::size(connectionPoints); i++)
	{
		const glm::vec2 slideOffset = platform.slideOffset * connectionPoints[i];
		connectionPointsWS[i] = baseTransform * glm::vec4(slideOffset.x, slideOffset.y, 0, 1);
	}
	
	return connectionPointsWS;
}

glm::vec3 ECPlatform::GetPosition(const eg::Entity& entity)
{
	return glm::vec3(GetPlatformTransform(entity)[3]);
}

eg::AABB ECPlatform::GetAABB(const eg::Entity& entity)
{
	const glm::mat4 transform = GetPlatformTransform(entity);
	const glm::vec3 world1 = transform * glm::vec4(-1.0f, -0.1f, -2.0f, 1.0f);
	const glm::vec3 world2 = transform * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
	return eg::AABB(world1, world2);
}

eg::Entity* ECPlatform::FindPlatform(const eg::AABB& searchAABB, eg::EntityManager& entityManager)
{
	for (eg::Entity& entity : entityManager.GetEntitySet(EntitySignature))
	{
		if (GetAABB(entity).Intersects(searchAABB))
			return &entity;
	}
	return nullptr;
}

inline void DrawPlatform(const eg::Entity& entity, eg::MeshBatch& meshBatch)
{
	const ECPlatform& platform = entity.GetComponent<ECPlatform>();
	const float slideDist = glm::length(platform.slideOffset);
	
	constexpr float SLIDER_MODEL_SCALE = 0.75f;
	constexpr float SLIDER_MODEL_LENGTH = 0.2f;
	const int numSliderInstances = std::round(slideDist / (SLIDER_MODEL_LENGTH * SLIDER_MODEL_SCALE));
	
	const glm::vec3 localFwd = glm::normalize(glm::vec3(platform.slideOffset.y, 0, -platform.slideOffset.x));
	const glm::vec3 localUp(0, 1, 0);
	const glm::mat4 sliderTransform = ECWallMounted::GetTransform(entity, 1.0f) *
		glm::mat4(glm::mat3(glm::cross(localUp, localFwd), localUp, localFwd) * SLIDER_MODEL_SCALE);
	
	for (int i = 1; i <= numSliderInstances; i++)
	{
		meshBatch.AddModel(*platformSliderModel, *platformSliderMaterial,
			glm::translate(sliderTransform, glm::vec3(0, 0, SLIDER_MODEL_LENGTH * i)));
	}
	
	meshBatch.AddModel(*platformModel, *platformMaterial, GetPlatformTransform(entity));
}

void ECPlatform::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
{
	DrawPlatform(entity, *message.meshBatch);
}

void ECPlatform::HandleMessage(eg::Entity& entity, const DrawMessage& message)
{
	DrawPlatform(entity, *message.meshBatch);
}

void ECPlatform::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
{
	ECEditorVisible::RenderDefaultSettings(entity);
	
	ImGui::DragFloat2("Slide Offset", &slideOffset.x, 0.1f);
	
	ImGui::DragFloat("Slide Time", &slideTime);
}

void ECPlatform::HandleMessage(eg::Entity& entity, const RayIntersectMessage& message)
{
	
}

void ECPlatform::HandleMessage(eg::Entity& entity, const CalculateCollisionMessage& message)
{
	//The platform's floor polygon in local space
	const glm::vec3 floorLocalCoords[] = { { -1, 0, 0 }, { 1, 0, 0 }, { 1, 0, -2 }, { -1, 0, -2 } };
	
	//Transforms the floor polygon to world space
	const glm::mat4 transform = GetPlatformTransform(entity);
	glm::vec3 floorWorldCoords[4];
	for (int i = 0; i < 4; i++)
	{
		floorWorldCoords[i] = transform * glm::vec4(floorLocalCoords[i], 1.0f);
	}
	
	CalcPolygonClipping(*message.clippingArgs, floorWorldCoords);
}

void ECPlatform::Update(const WorldUpdateArgs& args)
{
	for (eg::Entity& entity : args.world->EntityManager().GetEntitySet(EntitySignature))
	{
		ECPlatform& platform = entity.GetComponent<ECPlatform>();
		
		const glm::vec3 oldPos = GetPosition(entity);
		
		//Updates the slide progress
		const bool activated = entity.GetComponent<ECActivatable>().AllSourcesActive();
		const float slideDelta = args.dt / platform.slideTime;
		if (activated)
			platform.slideProgress = std::min(platform.slideProgress + slideDelta, 1.0f);
		else
			platform.slideProgress = std::max(platform.slideProgress - slideDelta, 0.0f);
		
		//Sets the move delta, which will be used by the player update function
		platform.moveDelta = GetPosition(entity) - oldPos;
		
		//Updates shadows if the platform moved
		if (glm::length2(platform.moveDelta) > 1E-5f)
		{
			args.invalidateShadows(eg::Sphere::CreateEnclosing(GetAABB(entity)));
		}
		
		static const glm::vec4 RIGID_BODY_OFFSET_LOCAL(0, -0.05f, -1, 1);
		
		ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
		btTransform rbTransform;
		rbTransform.setIdentity();
		rbTransform.setOrigin(bullet::FromGLM(glm::vec3(GetPlatformTransform(entity) * RIGID_BODY_OFFSET_LOCAL)));
		rigidBody.SetWorldTransform(rbTransform);
	}
}

struct PlatformSerializer : eg::IEntitySerializer
{
	std::string_view GetName() const override
	{
		return "Platform";
	}
	
	void Serialize(const eg::Entity& entity, std::ostream& stream) const override
	{
		gravity_pb::PlatformEntity platformPB;
		
		platformPB.set_dir((gravity_pb::Dir)entity.GetComponent<ECWallMounted>().wallUp);
		
		glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
		platformPB.set_posx(pos.x);
		platformPB.set_posy(pos.y);
		platformPB.set_posz(pos.z);
		
		const ECPlatform& platform = entity.GetComponent<ECPlatform>();
		platformPB.set_slideoffsetx(platform.slideOffset.x);
		platformPB.set_slideoffsety(platform.slideOffset.y);
		platformPB.set_slidetime(platform.slideTime);
		
		const ECActivatable& activatable = entity.GetComponent<ECActivatable>();
		platformPB.set_name(activatable.Name());
		platformPB.set_reqactivations(activatable.EnabledConnections());
		
		platformPB.SerializeToOstream(&stream);
	}
	
	void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
	{
		eg::Entity& entity = *ECPlatform::CreateEntity(entityManager);
		
		gravity_pb::PlatformEntity platformPB;
		platformPB.ParseFromIstream(&stream);
		
		glm::vec3 position(platformPB.posx(), platformPB.posy(), platformPB.posz());
		entity.InitComponent<eg::ECPosition3D>(position);
		entity.GetComponent<ECWallMounted>().wallUp = (Dir)platformPB.dir();
		
		ECPlatform& platform = entity.GetComponent<ECPlatform>();
		platform.slideOffset = glm::vec2(platformPB.slideoffsetx(), platformPB.slideoffsety());
		platform.slideTime = platformPB.slidetime();
		
		ECActivatable& activatable = entity.GetComponent<ECActivatable>();
		if (platformPB.name() != 0)
			activatable.SetName(platformPB.name());
		activatable.SetEnabledConnections(platformPB.reqactivations());
	}
};

eg::IEntitySerializer* ECPlatform::EntitySerializer = new PlatformSerializer;
