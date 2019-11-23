#include "CubeEnt.hpp"
#include "GooPlaneEnt.hpp"
#include "FloorButtonEnt.hpp"
#include "PlatformEnt.hpp"
#include "../../Player.hpp"
#include "../../BulletPhysics.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Protobuf/Build/CubeEntity.pb.h"
#include "ForceFieldEnt.hpp"
#include <imgui.h>

static constexpr float MASS = 1.0f;

static std::unique_ptr<btCollisionShape> collisionShape;

static eg::Model* cubeModel;
static eg::IMaterial* cubeMaterial;

static void OnInit()
{
	collisionShape = std::make_unique<btBoxShape>(btVector3(CubeEnt::RADIUS, CubeEnt::RADIUS, CubeEnt::RADIUS));
	
	cubeModel = &eg::GetAsset<eg::Model>("Models/Cube.obj");
	cubeMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Cube.yaml");
}

EG_ON_INIT(OnInit)

CubeEnt::CubeEnt(const glm::vec3& position, bool canFloat)
	: m_canFloat(canFloat)
{
	m_position = position;
	
	m_rigidBody.Init(MASS, *collisionShape);
	m_rigidBody.GetRigidBody()->setFlags(m_rigidBody.GetRigidBody()->getFlags() | BT_DISABLE_WORLD_GRAVITY);
	m_rigidBody.GetRigidBody()->setActivationState(DISABLE_DEACTIVATION);
	m_rigidBody.SetTransform(position, m_rotation);
	
	m_rigidBody.GetRigidBody()->setFriction(1.f);
	m_rigidBody.GetRigidBody()->setRollingFriction(.1);
	m_rigidBody.GetRigidBody()->setSpinningFriction(0.1);
	m_rigidBody.GetRigidBody()->setAnisotropicFriction(collisionShape->getAnisotropicRollingFrictionDirection(), btCollisionObject::CF_ANISOTROPIC_ROLLING_FRICTION);
}

void CubeEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::Checkbox("Float", &m_canFloat);
}

void CubeEnt::Spawned(bool isEditor)
{
	if (isEditor)
	{
		m_position += glm::vec3(DirectionVector(m_direction)) * (RADIUS + 0.01f);
	}
}

void CubeEnt::Draw(const EntDrawArgs& args)
{
	btTransform transform = m_rigidBody.GetBulletTransform();
	
	glm::mat4 worldMatrix;
	transform.getOpenGLMatrix(reinterpret_cast<float*>(&worldMatrix));
	worldMatrix *= glm::scale(glm::mat4(1), glm::vec3(RADIUS));
	
	args.meshBatch->AddModel(*cubeModel, *cubeMaterial, worldMatrix);
}

void CubeEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	const glm::mat4 worldMatrix =
		glm::translate(glm::mat4(1), m_position) *
		glm::mat4_cast(m_rotation) *
		glm::scale(glm::mat4(1), glm::vec3(RADIUS));
	
	args.meshBatch->AddModel(*cubeModel, *cubeMaterial, worldMatrix);
}

const void* CubeEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(RigidBodyComp))
		return &m_rigidBody;
	if (type == typeid(GravityBarrierInteractableComp))
		return &m_barrierInteractableComp;
	return Ent::GetComponent(type);
}

std::pair<bool, float> CubeEnt::RayIntersect(const eg::Ray& ray) const
{
	float distance;
	if (ray.Intersects(GetSphere(), distance) && distance > 0)
		return std::make_pair(true, distance);
	return std::make_pair(false, 0.0f);
}

void CubeEnt::CalculateCollision(Dir currentDown, struct ClippingArgs& args) const { }

void CubeEnt::Interact(Player& player)
{
	m_isPickedUp = !m_isPickedUp;
	player.SetIsCarrying(m_isPickedUp);
	
	if (!m_isPickedUp)
	{
		//Clear the cube's velocity so that it's impossible to fling the cube around.
		m_rigidBody.GetRigidBody()->setLinearVelocity(btVector3(0, 0, 0));
		m_rigidBody.GetRigidBody()->setAngularVelocity(btVector3(0, 0, 0));
	}
}

int CubeEnt::CheckInteraction(const Player& player) const
{
	if (!player.OnGround())
			return 0;
	
	//Very high so that dropping the cube has higher priority than other interactions.
	static constexpr int DROP_INTERACT_PRIORITY = 1000;
	if (m_isPickedUp)
		return DROP_INTERACT_PRIORITY;
	
	if (player.IsCarrying())
		return 0;
	
	static constexpr int PICK_UP_INTERACT_PRIORITY = 2;
	static constexpr float MAX_INTERACT_DIST = 1.5f;
	eg::Ray ray(player.Position(), player.Forward());
	float intersectDist;
	if (ray.Intersects(GetSphere(), intersectDist) && intersectDist > 0.0f && intersectDist < MAX_INTERACT_DIST)
	{
		return PICK_UP_INTERACT_PRIORITY;
	}
	
	return 0;
}

std::string_view CubeEnt::GetInteractDescription() const
{
	return "Pick Up";
}

static float* cubeBuoyancy = eg::TweakVarFloat("cube_buoyancy", 0.25f, 0.0f);
static float* cubeWaterDrag = eg::TweakVarFloat("cube_water_drag", 0.05f, 0.0f);

bool CubeEnt::SetGravity(Dir newGravity)
{
	if (m_isPickedUp)
		return false;
	m_currentDown = newGravity;
	return true;
}

void CubeEnt::Update(const WorldUpdateArgs& args)
{
	if (!args.player)
		return;
	
	eg::Sphere sphere(m_position, RADIUS);
	
	bool inGoo = false;
	args.world->entManager.ForEachOfType<GooPlaneEnt>([&] (const GooPlaneEnt& goo)
	{
		if (goo.IsUnderwater(sphere))
			inGoo = true;
	});
	
	if (inGoo)
	{
		args.world->entManager.RemoveEntity(*this);
		return;
	}
	
	std::optional<Dir> forceFieldGravity = ForceFieldEnt::CheckIntersection(args.world->entManager,
		eg::AABB(sphere.position - RADIUS, sphere.position + RADIUS));
	if (forceFieldGravity.has_value() && m_currentDown != *forceFieldGravity)
	{
		m_currentDown = *forceFieldGravity;
		if (m_isPickedUp)
		{
			m_isPickedUp = false;
			args.player->SetIsCarrying(false);
		}
	}
	
	const float impulseFactor = 1.0f / std::max(args.dt, 1.0f / 60.0f);
	
	if (m_isPickedUp)
	{
		auto [rbPos, rbRot] = m_rigidBody.GetTransform();
		m_position = rbPos;
		m_rotation = rbRot;
		
		constexpr float DIST_FROM_PLAYER = RADIUS + 0.7f; //Distance to the cube when carrying
		constexpr float MAX_DIST_FROM_PLAYER = RADIUS + 0.9f; //Drop the cube if further away than this
		
		glm::vec3 desiredPosition = args.player->EyePosition() + args.player->Forward() * DIST_FROM_PLAYER;
		
		eg::AABB desiredAABB(desiredPosition - RADIUS * 1.001f, desiredPosition + RADIUS * 1.001f);
		
		glm::vec3 deltaPos = desiredPosition - m_position;
		
		if (glm::length2(deltaPos) > MAX_DIST_FROM_PLAYER * MAX_DIST_FROM_PLAYER)
		{
			m_isPickedUp = false;
			args.player->SetIsCarrying(false);
		}
		else
		{
			ClippingArgs clipArgs;
			clipArgs.ellipsoid.center = m_position;
			clipArgs.ellipsoid.radii = glm::vec3(RADIUS * 0.75f);
			clipArgs.move = deltaPos;
			args.world->CalcClipping(clipArgs, m_currentDown);
			
			if (clipArgs.collisionInfo.collisionFound)
				deltaPos *= clipArgs.collisionInfo.distance;
			
			m_rigidBody.GetRigidBody()->setGravity(btVector3(0, 0, 0));
			m_rigidBody.GetRigidBody()->setLinearVelocity(bullet::FromGLM(deltaPos * impulseFactor));
			m_rigidBody.GetRigidBody()->setAngularVelocity(btVector3(0, 0, 0));
			m_rigidBody.GetRigidBody()->clearForces();
		}
	}
	else
	{
		//Updates the gravity vector
		glm::vec3 gravityUnit = glm::vec3(DirectionVector(m_currentDown));
		m_rigidBody.GetRigidBody()->setGravity(bullet::FromGLM(gravityUnit * bullet::GRAVITY));
		
		//Water interaction
		if (m_canFloat && m_waterQueryAABB != nullptr)
		{
			WaterSimulator::QueryResults waterQueryRes = m_waterQueryAABB->GetResults();
			
			glm::vec3 buoyancy = waterQueryRes.buoyancy * *cubeBuoyancy;
			m_rigidBody.GetRigidBody()->applyCentralForce(bullet::FromGLM(buoyancy));
			float damping = std::min(waterQueryRes.numIntersecting * *cubeWaterDrag, 0.5f);
			m_rigidBody.GetRigidBody()->setDamping(damping, damping);
		}
		
		btBroadphaseProxy* bpProxy = m_rigidBody.GetRigidBody()->getBroadphaseProxy();
		bpProxy->m_collisionFilterGroup = 1 | (2 << ((int)m_currentDown / 2));
	}
	
	m_barrierInteractableComp.currentDown = m_currentDown;
}

void CubeEnt::UpdatePostSim(const WorldUpdateArgs& args)
{
	auto [rbPos, rbRot] = m_rigidBody.GetTransform();
	m_position = rbPos;
	m_rotation = rbRot;
	
	if (m_rigidBody.GetRigidBody()->getLinearVelocity().length2() > 1E-4f ||
	    m_rigidBody.GetRigidBody()->getAngularVelocity().length2() > 1E-4f)
	{
		args.invalidateShadows(GetSphere());
	}
	
	const glm::vec3 down(DirectionVector(m_currentDown));
	const eg::AABB cubeAABB(m_position - RADIUS, m_position + RADIUS);
	
	args.world->entManager.ForEachOfType<FloorButtonEnt>([&] (FloorButtonEnt& buttonEntity)
	{
		glm::vec3 toButton = glm::normalize(buttonEntity.Pos() - m_position);
		if (glm::dot(toButton, down) > 0.1f && buttonEntity.GetAABB().Intersects(cubeAABB))
			buttonEntity.Activate();
	});
	
	if (m_canFloat)
	{
		if (m_waterQueryAABB == nullptr)
		{
			m_waterQueryAABB = std::make_shared<WaterSimulator::QueryAABB>();
			args.waterSim->AddQueryAABB(m_waterQueryAABB);
		}
		
		m_waterQueryAABB->SetAABB(cubeAABB);
	}
}

void CubeEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::CubeEntity cubePB;
	
	SerializePos(cubePB);
	
	cubePB.set_rotationx(m_rotation.x);
	cubePB.set_rotationy(m_rotation.y);
	cubePB.set_rotationz(m_rotation.z);
	cubePB.set_rotationw(m_rotation.w);
	
	cubePB.set_can_float(m_canFloat);
	
	cubePB.SerializeToOstream(&stream);
}

void CubeEnt::Deserialize(std::istream& stream)
{
	gravity_pb::CubeEntity cubePB;
	cubePB.ParseFromIstream(&stream);
	
	DeserializePos(cubePB);
	
	m_rotation = glm::quat(cubePB.rotationw(), cubePB.rotationx(), cubePB.rotationy(), cubePB.rotationz());
	m_canFloat = cubePB.can_float();
	
	m_rigidBody.SetTransform(m_position, m_rotation);
}
