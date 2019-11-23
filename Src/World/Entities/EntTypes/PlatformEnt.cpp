#include "PlatformEnt.hpp"
#include "../../BulletPhysics.hpp"
#include "../../Entities/EntityManager.hpp"
#include "../../WorldUpdateArgs.hpp"
#include "../../Clipping.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Protobuf/Build/PlatformEntity.pb.h"

#include <imgui.h>

static eg::Model* platformModel;
static eg::IMaterial* platformMaterial;

static eg::Model* platformSliderModel;
static eg::IMaterial* platformSliderMaterial;

static std::unique_ptr<btCollisionShape> platformCollisionShape;

static constexpr float RIGID_BODY_HEIGHT = 0.2f;

static void OnInit()
{
	platformModel = &eg::GetAsset<eg::Model>("Models/Platform.obj");
	platformMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Platform.yaml");
	
	platformSliderModel = &eg::GetAsset<eg::Model>("Models/PlatformSlider.obj");
	platformSliderMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/PlatformSlider.yaml");
	
	platformCollisionShape = std::make_unique<btBoxShape>(btVector3(1.0f, RIGID_BODY_HEIGHT, 1.0f));
}

EG_ON_INIT(OnInit)

PlatformEnt::PlatformEnt()
	: m_activatable(&PlatformEnt::GetConnectionPoints)
{
	m_rigidBody.InitStatic(*platformCollisionShape);
	m_rigidBody.GetRigidBody()->setCollisionFlags(m_rigidBody.GetRigidBody()->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	m_rigidBody.GetRigidBody()->setActivationState(DISABLE_DEACTIVATION);
	m_rigidBody.GetRigidBody()->setFriction(0.5f);
}

static const glm::mat4 PLATFORM_ROTATION_MATRIX(0, 0, -1, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1);

//Gets the transformation matrix for the platform without any sliding
inline glm::mat4 GetPlatformBaseTransform(const Ent& entity)
{
	return entity.GetTransform(1.0f) * PLATFORM_ROTATION_MATRIX;
}

//Gets the transformation matrix for the platform, slided to it's current position
glm::mat4 PlatformEnt::GetPlatformTransform() const
{
	const glm::vec2 slideOffset = m_slideOffset * glm::smoothstep(0.0f, 1.0f, m_slideProgress);
	return glm::translate(GetPlatformBaseTransform(*this), glm::vec3(slideOffset.x, slideOffset.y, 0));
}

std::vector<glm::vec3> PlatformEnt::GetConnectionPoints(const Ent& entity)
{
	const glm::mat4 baseTransform = GetPlatformBaseTransform(entity);
	const PlatformEnt& platform = static_cast<const PlatformEnt&>(entity);
	
	const float connectionPoints[] = { 0.0f, 0.5f, 1.0f };
	
	std::vector<glm::vec3> connectionPointsWS(std::size(connectionPoints));
	for (size_t i = 0; i < std::size(connectionPoints); i++)
	{
		const glm::vec2 slideOffset = platform.m_slideOffset * connectionPoints[i];
		connectionPointsWS[i] = baseTransform * glm::vec4(slideOffset.x, slideOffset.y, 0, 1);
	}
	
	return connectionPointsWS;
}

glm::vec3 PlatformEnt::GetPlatformPosition() const
{
	return glm::vec3(GetPlatformTransform() * glm::vec4(0, RIGID_BODY_HEIGHT / 2, 0, 1));
}

void PlatformEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::DragFloat2("Slide Offset", &m_slideOffset.x, 0.1f);
	
	ImGui::DragFloat("Slide Time", &m_slideTime);
}

void PlatformEnt::DrawGeneral(eg::MeshBatch& meshBatch) const
{
	const float slideDist = glm::length(m_slideOffset);
	
	constexpr float SLIDER_MODEL_SCALE = 0.5f;
	constexpr float SLIDER_MODEL_LENGTH = 0.2f;
	const int numSliderInstances = (int)std::round(slideDist / (SLIDER_MODEL_LENGTH * SLIDER_MODEL_SCALE));
	
	const glm::vec3 localFwd = glm::normalize(glm::vec3(m_slideOffset.y, 0, -m_slideOffset.x));
	const glm::vec3 localUp(0, 1, 0);
	const glm::mat4 sliderTransform = GetTransform(1.0f) *
		glm::mat4(glm::mat3(glm::cross(localUp, localFwd), localUp, localFwd) * SLIDER_MODEL_SCALE);
	
	for (int i = 1; i <= numSliderInstances; i++)
	{
		meshBatch.AddModel(*platformSliderModel, *platformSliderMaterial,
			glm::translate(sliderTransform, glm::vec3(0, 0, SLIDER_MODEL_LENGTH * (float)i)));
	}
	
	meshBatch.AddModel(*platformModel, *platformMaterial, GetPlatformTransform());
}

void PlatformEnt::Draw(const EntDrawArgs& args)
{
	DrawGeneral(*args.meshBatch);
}

void PlatformEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	DrawGeneral(*args.meshBatch);
}

void PlatformEnt::Update(const WorldUpdateArgs& args)
{
	if (args.player == nullptr)
		return;
	
	const glm::vec3 oldPos = GetPlatformPosition();
	
	//Updates the slide progress
	const bool activated = m_activatable.AllSourcesActive();
	const float slideDelta = args.dt / m_slideTime;
	if (activated)
		m_slideProgress = std::min(m_slideProgress + slideDelta, 1.0f);
	else
		m_slideProgress = std::max(m_slideProgress - slideDelta, 0.0f);
	
	//Sets the move delta, which will be used by the player update function
	m_moveDelta = GetPlatformPosition() - oldPos;
	
	//Updates shadows if the platform moved
	if (glm::length2(m_moveDelta) > 1E-5f)
	{
		args.invalidateShadows(eg::Sphere::CreateEnclosing(GetPlatformAABB()));
	}
	
	static const glm::vec4 RIGID_BODY_OFFSET_LOCAL(0, -RIGID_BODY_HEIGHT, -1, 1);
	
	btTransform rbTransform;
	rbTransform.setIdentity();
	rbTransform.setOrigin(bullet::FromGLM(glm::vec3(GetPlatformTransform() * RIGID_BODY_OFFSET_LOCAL)));
	m_rigidBody.SetWorldTransform(rbTransform);
}

PlatformEnt* PlatformEnt::FindPlatform(const eg::AABB& searchAABB, EntityManager& entityManager)
{
	PlatformEnt* result = nullptr;
	entityManager.ForEachOfType<PlatformEnt>([&] (PlatformEnt& platform)
	{
		if (platform.GetPlatformAABB().Intersects(searchAABB))
			result = &platform;
	});
	return result;
}

eg::AABB PlatformEnt::GetPlatformAABB() const
{
	const glm::mat4 transform = GetPlatformTransform();
	const glm::vec3 world1 = transform * glm::vec4(-1.0f, -0.1f, -2.0f, 1.0f);
	const glm::vec3 world2 = transform * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
	return eg::AABB(world1, world2);
}

std::pair<bool, float> PlatformEnt::RayIntersect(const eg::Ray& ray) const
{
	return std::make_pair(false, 0.0f);
}

void PlatformEnt::CalculateCollision(Dir currentDown, ClippingArgs& args) const
{
	//The platform's floor polygon in local space
	const glm::vec3 floorLocalCoords[] = { { -1, 0, 0 }, { 1, 0, 0 }, { 1, 0, -2 }, { -1, 0, -2 } };
	
	//Transforms the floor polygon to world space
	const glm::mat4 transform = GetPlatformTransform();
	glm::vec3 floorWorldCoords[4];
	for (int i = 0; i < 4; i++)
	{
		floorWorldCoords[i] = transform * glm::vec4(floorLocalCoords[i], 1.0f);
	}
	
	CalcPolygonClipping(args, floorWorldCoords);
}

const void* PlatformEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(RigidBodyComp))
		return &m_rigidBody;
	if (type == typeid(ActivatableComp))
		return &m_activatable;
	return Ent::GetComponent(type);
}

void PlatformEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::PlatformEntity platformPB;
	
	platformPB.set_dir((gravity_pb::Dir)m_direction);
	SerializePos(platformPB);
	
	platformPB.set_slide_offset_x(m_slideOffset.x);
	platformPB.set_slide_offset_y(m_slideOffset.y);
	platformPB.set_slide_time(m_slideTime);
	
	platformPB.set_name(m_activatable.m_name);
	
	platformPB.SerializeToOstream(&stream);
}

void PlatformEnt::Deserialize(std::istream& stream)
{
	gravity_pb::PlatformEntity platformPB;
	platformPB.ParseFromIstream(&stream);
	
	DeserializePos(platformPB);
	m_direction = (Dir)platformPB.dir();
	
	m_slideOffset = glm::vec2(platformPB.slide_offset_x(), platformPB.slide_offset_y());
	m_slideTime = platformPB.slide_time();
	
	if (platformPB.name() != 0)
		m_activatable.m_name = platformPB.name();
}
