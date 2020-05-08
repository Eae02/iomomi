#include "CubeEnt.hpp"
#include "GooPlaneEnt.hpp"
#include "PlatformEnt.hpp"
#include "FloorButtonEnt.hpp"
#include "../../Player.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Protobuf/Build/CubeEntity.pb.h"
#include "ForceFieldEnt.hpp"
#include <imgui.h>

static constexpr float MASS = 50.0f;

static eg::Model* cubeModel;
static eg::IMaterial* cubeMaterial;

static eg::Model* woodCubeModel;
static eg::IMaterial* woodCubeMaterial;

static void OnInit()
{
	cubeModel = &eg::GetAsset<eg::Model>("Models/Cube.obj");
	cubeMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Cube.yaml");
	woodCubeModel = &eg::GetAsset<eg::Model>("Models/WoodCube.obj");
	woodCubeMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/WoodCube.yaml");
}

EG_ON_INIT(OnInit)

CubeEnt::CubeEnt(const glm::vec3& position, bool _canFloat)
	: canFloat(_canFloat)
{
	m_physicsObject.position = position;
	m_physicsObject.mass = MASS;
	m_physicsObject.shape = eg::AABB(-glm::vec3(RADIUS), glm::vec3(RADIUS));
	m_physicsObject.owner = this;
	m_physicsObject.canCarry = true;
	m_physicsObject.debugColor = 0x1f9bde;
	m_physicsObject.shouldCollide = ShouldCollide;
}

bool CubeEnt::ShouldCollide(const PhysicsObject& self, const PhysicsObject& other)
{
	CubeEnt* cube = (CubeEnt*)std::get<Ent*>(self.owner);
	if (cube->m_isPickedUp && std::holds_alternative<Player*>(other.owner))
		return false;
	return true;
}

void CubeEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::Checkbox("Float", &canFloat);
}

void CubeEnt::Spawned(bool isEditor)
{
	if (isEditor)
	{
		m_physicsObject.position += glm::vec3(DirectionVector(m_direction)) * (RADIUS + 0.01f);
		m_physicsObject.position = m_physicsObject.position;
	}
}

void CubeEnt::Draw(eg::MeshBatch& meshBatch, const glm::mat4& transform) const
{
	meshBatch.AddModel(
		*(canFloat ? woodCubeModel : cubeModel),
		*(canFloat ? woodCubeMaterial : cubeMaterial), StaticPropMaterial::InstanceData(transform));
}

void CubeEnt::GameDraw(const EntGameDrawArgs& args)
{
	glm::mat4 worldMatrix = glm::translate(glm::mat4(1), m_physicsObject.displayPosition);
	worldMatrix *= glm::scale(glm::mat4(1), glm::vec3(RADIUS));
	Draw(*args.meshBatch, worldMatrix);
}

void CubeEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	const glm::mat4 worldMatrix =
		glm::translate(glm::mat4(1), m_physicsObject.displayPosition) *
		glm::mat4_cast(m_physicsObject.rotation) *
		glm::scale(glm::mat4(1), glm::vec3(RADIUS));
	Draw(*args.meshBatch, worldMatrix);
}

const void* CubeEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(GravityBarrierInteractableComp))
		return &m_barrierInteractableComp;
	return Ent::GetComponent(type);
}

void CubeEnt::Interact(Player& player)
{
	m_isPickedUp = !m_isPickedUp;
	player.m_isCarrying = m_isPickedUp;
	
	if (!m_isPickedUp)
	{
		//Clear the cube's velocity so that it's impossible to fling the cube around.
		m_physicsObject.velocity = { };
	}
}

int CubeEnt::CheckInteraction(const Player& player, const class PhysicsEngine& physicsEngine) const
{
	if (!player.OnGround() && !player.Underwater())
		return 0;
	
	//Very high so that dropping the cube has higher priority than other interactions.
	static constexpr int DROP_INTERACT_PRIORITY = 1000;
	if (m_isPickedUp)
		return DROP_INTERACT_PRIORITY;
	
	if (player.m_isCarrying)
		return 0;
	
	static constexpr int PICK_UP_INTERACT_PRIORITY = 2;
	static constexpr float MAX_INTERACT_DIST = 1.5f;
	eg::Ray ray(player.EyePosition(), player.Forward());
	
	OrientedBox box;
	box.center = m_physicsObject.position;
	box.rotation = m_physicsObject.rotation;
	box.radius = glm::vec3(RADIUS);
	auto [intersectObj, intersectDist] = physicsEngine.RayIntersect(ray, RAY_MASK_BLOCK_PICK_UP);
	
	if (intersectObj == &m_physicsObject && intersectDist < MAX_INTERACT_DIST)
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
static float* cubeButtonMinAttractDist = eg::TweakVarFloat("cube_ba_min_dist", 1E-3f, 0.0f);
static float* cubeButtonMaxAttractDist = eg::TweakVarFloat("cube_ba_dist", 0.8f, 0.0f);
static float* cubeButtonAttractForce = eg::TweakVarFloat("cube_ba_force", 1.5f, 0.0f);

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
	{
		m_physicsObject.displayPosition = m_position;
		return;
	}
	
	eg::Sphere sphere(m_physicsObject.position, RADIUS);
	
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
			args.player->m_isCarrying = false;
		}
	}
	
	if (m_isPickedUp)
	{
		constexpr float DIST_FROM_PLAYER = RADIUS + 0.7f; //Distance to the cube when carrying
		constexpr float MAX_DIST_FROM_PLAYER = RADIUS + 0.9f; //Drop the cube if further away than this
		
		glm::vec3 desiredPosition = args.player->EyePosition() + args.player->Forward() * DIST_FROM_PLAYER;
		
		eg::AABB desiredAABB(desiredPosition - RADIUS * 1.001f, desiredPosition + RADIUS * 1.001f);
		
		glm::vec3 deltaPos = desiredPosition - m_physicsObject.position;
		m_physicsObject.gravity = { };
		
		if (glm::length2(deltaPos) > MAX_DIST_FROM_PLAYER * MAX_DIST_FROM_PLAYER)
		{
			//The cube has moved to far away from the player, so it should be dropped.
			m_isPickedUp = false;
			args.player->m_isCarrying = false;
		}
		else
		{
			m_physicsObject.move = deltaPos;
			m_physicsObject.velocity = { };
		}
	}
	else
	{
		m_physicsObject.gravity = DirectionVector(m_currentDown);
		
		//Water interaction
		if (canFloat && m_waterQueryAABB != nullptr)
		{
			WaterSimulator::QueryResults waterQueryRes = m_waterQueryAABB->GetResults();
			
			glm::vec3 buoyancy = waterQueryRes.buoyancy * *cubeBuoyancy;
			m_physicsObject.force += buoyancy;
			//m_rigidBody.GetRigidBody()->applyCentralForce(bullet::FromGLM(buoyancy));
			//float damping = std::min(waterQueryRes.numIntersecting * *cubeWaterDrag, 0.5f);
			//m_rigidBody.GetRigidBody()->setDamping(damping, damping);
		}
		else
		{
			const float maxDist2 = *cubeButtonMaxAttractDist * *cubeButtonMaxAttractDist;
			
			const FloorButtonEnt* currentButton = nullptr;
			args.world->entManager.ForEachOfType<FloorButtonEnt>([&] (const FloorButtonEnt& floorButton)
			{
				if (floorButton.Direction() == OppositeDir(m_currentDown) &&
				    glm::distance2(floorButton.Pos(), Pos()) < maxDist2)
				{
					currentButton = &floorButton;
				}
			});
			
			if (currentButton != nullptr)
			{
				const float minDist2 = *cubeButtonMinAttractDist * *cubeButtonMinAttractDist;
				
				glm::vec3 toCenter = currentButton->Pos() - Pos();
				glm::vec3 buttonDir(DirectionVector(currentButton->Direction()));
				toCenter -= buttonDir * glm::dot(toCenter, buttonDir);
				if (glm::length2(toCenter) > minDist2)
				{
					toCenter = glm::normalize(toCenter) * *cubeButtonAttractForce;
					m_physicsObject.force += toCenter;
				}
			}
		}
	}
	
	m_barrierInteractableComp.currentDown = m_currentDown;
}

void CubeEnt::UpdatePostSim(const WorldUpdateArgs& args)
{
	if (glm::distance(m_previousPosition, m_physicsObject.position) > 1E-3f)
	{
		m_previousPosition = m_physicsObject.position;
		args.invalidateShadows(GetSphere());
	}
	
	const glm::vec3 down(DirectionVector(m_currentDown));
	const eg::AABB cubeAABB(m_physicsObject.position - RADIUS, m_physicsObject.position + RADIUS);
	
	if (canFloat)
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
	
	cubePB.set_posx(m_physicsObject.position.x);
	cubePB.set_posy(m_physicsObject.position.y);
	cubePB.set_posz(m_physicsObject.position.z);
	
	cubePB.set_rotationx(m_physicsObject.rotation.x);
	cubePB.set_rotationy(m_physicsObject.rotation.y);
	cubePB.set_rotationz(m_physicsObject.rotation.z);
	cubePB.set_rotationw(m_physicsObject.rotation.w);
	
	cubePB.set_can_float(canFloat);
	
	cubePB.SerializeToOstream(&stream);
}

void CubeEnt::Deserialize(std::istream& stream)
{
	gravity_pb::CubeEntity cubePB;
	cubePB.ParseFromIstream(&stream);
	
	m_position = m_physicsObject.position = glm::vec3(cubePB.posx(), cubePB.posy(), cubePB.posz());
	m_physicsObject.rotation = glm::quat(cubePB.rotationw(), cubePB.rotationx(), cubePB.rotationy(), cubePB.rotationz());
	canFloat = cubePB.can_float();
}

void CubeEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}
