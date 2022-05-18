#include "CubeEnt.hpp"
#include "FloorButtonEnt.hpp"
#include "../GooPlaneEnt.hpp"
#include "../ForceFieldEnt.hpp"
#include "../../../Player.hpp"
#include "../../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Graphics/Materials/GravityIndicatorMaterial.hpp"
#include "../../../../Settings.hpp"
#include "../../../../../Protobuf/Build/CubeEntity.pb.h"
#include <imgui.h>

static float* cubeBuoyancyScale = eg::TweakVarFloat("cube_bcy_scale", 0.5f, 0.0f);
static float* cubeBuoyancyLimit = eg::TweakVarFloat("cube_bcy_lim", 25.0f, 0.0f);
static float* cubeWaterDrag = eg::TweakVarFloat("cube_water_drag", 0.5f, 0.0f);
static float* cubeWaterPullLimit = eg::TweakVarFloat("cube_pull_lim", 2.0f, 0.0f);
static float* cubeMass = eg::TweakVarFloat("cube_mass", 50.0f, 0.0f);
static float* cubeCarryDist = eg::TweakVarFloat("cube_carry_dist", 0.7f, 0.0f);
static float* cubeDropDist = eg::TweakVarFloat("cube_drop_dist", 0.9f, 0.0f);
static float* cubeMaxInteractDist = eg::TweakVarFloat("cube_max_interact_dist", 1.5f, 0.0f);

static eg::Model* cubeModel;
static eg::CollisionMesh cubeSelColMesh;
static eg::IMaterial* cubeMaterial;

static eg::Model* woodCubeModel;
static eg::CollisionMesh woodCubeSelColMesh;
static eg::IMaterial* woodCubeMaterial;

static void OnInit()
{
	cubeModel = &eg::GetAsset<eg::Model>("Models/Cube.aa.obj");
	cubeSelColMesh = cubeModel->MakeCollisionMesh(0);
	cubeMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Cube.yaml");
	woodCubeModel = &eg::GetAsset<eg::Model>("Models/WoodCube.aa.obj");
	woodCubeSelColMesh = cubeModel->MakeCollisionMesh(0);
	woodCubeMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/WoodCube.yaml");
}

EG_ON_INIT(OnInit)

CubeEnt::CubeEnt(const glm::vec3& position, bool _canFloat)
	: canFloat(_canFloat)
{
	m_physicsObject.position = position;
	m_physicsObject.displayPosition = position;
	m_physicsObject.mass = *cubeMass;
	m_physicsObject.shape = eg::AABB(-glm::vec3(RADIUS), glm::vec3(RADIUS));
	m_physicsObject.owner = this;
	m_physicsObject.canCarry = true;
	m_physicsObject.debugColor = 0x1f9bde;
	m_physicsObject.shouldCollide = ShouldCollide;
	m_physicsObject.rayIntersectMask = RAY_MASK_BLOCK_GUN | RAY_MASK_BLOCK_PICK_UP;
}

bool CubeEnt::ShouldCollide(const PhysicsObject& self, const PhysicsObject& other)
{
	CubeEnt* cube = (CubeEnt*)std::get<Ent*>(self.owner);
	if (cube->m_collisionWithPlayerDisabled && std::holds_alternative<Player*>(other.owner))
		return false;
	if (cube->isSpawning && std::holds_alternative<World*>(other.owner))
		return false;
	return true;
}

void CubeEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::Checkbox("Float", &canFloat);
	ImGui::Checkbox("Gravity control hint", &m_showChangeGravityControlHint);
}

void CubeEnt::Draw(eg::MeshBatch& meshBatch, const glm::mat4& transform) const
{
	meshBatch.AddModel(
		*(canFloat ? woodCubeModel : cubeModel),
		*(canFloat ? woodCubeMaterial : cubeMaterial), StaticPropMaterial::InstanceData(transform));
}

float* cubeGAIntenstiyMax = eg::TweakVarFloat("cube_ga_imax", 2.0f, 0);
float* cubeGAIntenstiyMin = eg::TweakVarFloat("cube_ga_imin", 0.1f, 0);
float* cubeGAIntenstiyFlash = eg::TweakVarFloat("cube_ga_iflash", 4.0f, 0);
float* cubeGAFlashTime = eg::TweakVarFloat("cube_ga_flash_time", 0.3f, 0);
float* cubeGAPulseTime = eg::TweakVarFloat("cube_ga_pulse_time", 2, 0);

void CubeEnt::GameDraw(const EntGameDrawArgs& args)
{
	if (!args.frustum->Intersects(GetSphere()))
		return;
	
	glm::mat4 worldMatrix = glm::translate(glm::mat4(1), m_physicsObject.displayPosition);
	worldMatrix *= glm::scale(glm::mat4(1), glm::vec3(RADIUS));
	Draw(*args.meshBatch, worldMatrix);
	
	if (!m_gravityHasChanged)
		return;
	
	GravityIndicatorMaterial::InstanceData instanceData(worldMatrix);
	instanceData.down = glm::vec3(DirectionVector(m_currentDown));
	
	float flashIntensity = settings.gunFlash ? *cubeGAIntenstiyFlash : 1.0f;
	float t = RenderSettings::instance->gameTime * eg::TWO_PI / *cubeGAPulseTime;
	float intensity = glm::mix(0.25f, 1.0f, std::sin(t) * 0.5f + 0.5f);
	instanceData.minIntensity = glm::mix(*cubeGAIntenstiyMin * intensity, flashIntensity, m_gravityIndicatorFlashIntensity);
	instanceData.maxIntensity = glm::mix(*cubeGAIntenstiyMax * intensity, flashIntensity, m_gravityIndicatorFlashIntensity);
	
	args.meshBatch->AddModel(*(canFloat ? woodCubeModel : cubeModel), GravityIndicatorMaterial::instance, instanceData);
}

glm::mat4 CubeEnt::GetEditorWorldMatrix() const
{
	return
		glm::translate(glm::mat4(1), m_physicsObject.position) *
		glm::mat4_cast(m_physicsObject.rotation) *
		glm::scale(glm::mat4(1), glm::vec3(RADIUS));
}

void CubeEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	Draw(*args.meshBatch, GetEditorWorldMatrix());
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
	if ((!m_isPickedUp && !player.CanPickUpCube()) || isSpawning)
		return 0;
	
	//Very high so that dropping the cube has higher priority than other interactions.
	static constexpr int DROP_INTERACT_PRIORITY = 1000;
	if (m_isPickedUp)
		return DROP_INTERACT_PRIORITY;
	
	if (player.m_isCarrying)
		return 0;
	
	static constexpr int PICK_UP_INTERACT_PRIORITY = 2;
	eg::Ray ray(player.EyePosition(), player.Forward());
	
	OrientedBox box;
	box.center = m_physicsObject.position;
	box.rotation = m_physicsObject.rotation;
	box.radius = glm::vec3(RADIUS);
	auto [intersectObj, intersectDist] = physicsEngine.RayIntersect(ray, RAY_MASK_BLOCK_PICK_UP);
	
	if (intersectObj == &m_physicsObject && intersectDist < *cubeMaxInteractDist)
	{
		return PICK_UP_INTERACT_PRIORITY;
	}
	
	return 0;
}

std::optional<InteractControlHint> CubeEnt::GetInteractControlHint() const
{
	InteractControlHint hint;
	hint.keyBinding = &settings.keyInteract;
	hint.message = m_isPickedUp ? "Drop" : "Pick Up";
	hint.optControlHintType = OptionalControlHintType::PickUpCube;
	return hint;
}

bool CubeEnt::SetGravity(Dir newGravity)
{
	if (m_isPickedUp)
		return false;
	m_currentDown = newGravity;
	m_gravityHasChanged = true;
	if (m_gravityIndicatorFlashIntensity < 1E-3f)
		m_gravityIndicatorFlashIntensity = 1;
	return true;
}

bool CubeEnt::ShouldShowGravityBeamControlHint(Dir newGravity)
{
	return !m_isPickedUp && newGravity != m_currentDown && m_showChangeGravityControlHint;
}

void CubeEnt::Update(const WorldUpdateArgs& args)
{
	if (args.mode == WorldMode::Editor || args.mode == WorldMode::Thumbnail)
		return;
	
	m_gravityIndicatorFlashIntensity = std::max(m_gravityIndicatorFlashIntensity - args.dt / *cubeGAFlashTime, 0.0f);
	
	m_physicsObject.mass = *cubeMass;
	
	eg::Sphere sphere(m_physicsObject.position, RADIUS);
	eg::AABB aabb(sphere.position - RADIUS, sphere.position + RADIUS);
	
	bool inGoo = false;
	args.world->entManager.ForEachOfType<GooPlaneEnt>([&] (const GooPlaneEnt& goo)
	{
		if (goo.IsUnderwater(sphere))
			inGoo = true;
	});
	
	if (inGoo)
	{
		if (args.plShadowMapper)
		{
			args.plShadowMapper->Invalidate(GetSphere());
		}
		args.world->entManager.RemoveEntity(*this);
		return;
	}
	
	std::optional<Dir> forceFieldGravity = ForceFieldEnt::CheckIntersection(args.world->entManager, aabb);
	if (forceFieldGravity.has_value() && m_currentDown != *forceFieldGravity)
	{
		if (m_isPickedUp && args.player)
		{
			m_isPickedUp = false;
			args.player->m_isCarrying = false;
		}
		SetGravity(*forceFieldGravity);
	}
	
	if (isSpawning || (args.waterSim && !args.waterSim->IsPresimComplete()))
	{
		m_physicsObject.gravity = { };
		m_collisionWithPlayerDisabled = true;
	}
	else if (!m_isPickedUp)
	{
		if (m_collisionWithPlayerDisabled && args.player && !args.player->GetAABB().Intersects(aabb))
		{
			m_collisionWithPlayerDisabled = false;
		}
		
		m_physicsObject.gravity = DirectionVector(m_currentDown);
		
		//Water interaction
		if (m_waterQueryAABB != nullptr)
		{
			IWaterSimulator::QueryResults waterQueryRes = m_waterQueryAABB->GetResults();
			
			glm::vec3 relVelocity = waterQueryRes.waterVelocity - m_physicsObject.velocity;
			glm::vec3 pullForce = relVelocity * *cubeWaterDrag;
			float pullForceLen = glm::length(pullForce);
			if (pullForceLen > *cubeWaterPullLimit)
			{
				pullForce *= *cubeWaterPullLimit / pullForceLen;
			}
			m_physicsObject.force += pullForce;
			
			if (canFloat)
			{
				glm::vec3 buoyancyForce = waterQueryRes.buoyancy * *cubeBuoyancyScale;
				float buoyancyLen = glm::length(buoyancyForce);
				if (buoyancyLen > *cubeBuoyancyLimit)
				{
					buoyancyForce *= *cubeBuoyancyLimit / buoyancyLen;
				}
				m_physicsObject.force += buoyancyForce;
			}
		}
	}
	
	m_barrierInteractableComp.currentDown = m_currentDown;
}

void CubeEnt::UpdateAfterPlayer(const WorldUpdateArgs& args)
{
	if (m_isPickedUp)
	{
		const float distFromPlayer = RADIUS + *cubeCarryDist; //Distance to the cube when carrying
		const float maxDistFromPlayer = RADIUS + *cubeDropDist; //Drop the cube if further away than this
		
		glm::mat4 viewMatrix, viewMatrixInv;
		args.player->GetViewMatrix(viewMatrix, viewMatrixInv);
		
		glm::vec3 desiredPosition = viewMatrixInv * glm::vec4(0, 0, -distFromPlayer, 1);
		
		eg::AABB desiredAABB(desiredPosition - RADIUS * 1.001f, desiredPosition + RADIUS * 1.001f);
		
		glm::vec3 deltaPos = desiredPosition - m_physicsObject.position;
		m_physicsObject.gravity = { };
		
		if (glm::length2(deltaPos) > maxDistFromPlayer * maxDistFromPlayer)
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
		
		m_collisionWithPlayerDisabled = true;
	}
}

void CubeEnt::UpdateAfterSimulation(const WorldUpdateArgs& args)
{
	if (args.mode == WorldMode::Editor || args.mode == WorldMode::Thumbnail)
		return;
	
	if (settings.shadowQuality >= QualityLevel::Medium &&
		glm::distance(m_previousPosition, m_physicsObject.displayPosition) > 1E-3f)
	{
		m_previousPosition = m_physicsObject.displayPosition;
		if (args.plShadowMapper)
		{
			args.plShadowMapper->Invalidate(GetSphere());
		}
	}
	
	const glm::vec3 down(DirectionVector(m_currentDown));
	const eg::AABB cubeAABB(m_physicsObject.position - RADIUS, m_physicsObject.position + RADIUS);
	
	if (m_waterQueryAABB == nullptr && args.waterSim != nullptr)
	{
		m_waterQueryAABB = args.waterSim->AddQueryAABB(cubeAABB);
	}
	else if (m_waterQueryAABB != nullptr)
	{
		m_waterQueryAABB->SetAABB(cubeAABB);
	}
}

void CubeEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::CubeEntity cubePB;
	
	SerializePos(cubePB, m_physicsObject.position);
	
	cubePB.set_rotationx(m_physicsObject.rotation.x);
	cubePB.set_rotationy(m_physicsObject.rotation.y);
	cubePB.set_rotationz(m_physicsObject.rotation.z);
	cubePB.set_rotationw(m_physicsObject.rotation.w);
	
	cubePB.set_show_change_gravity_control_hint(m_showChangeGravityControlHint);
	cubePB.set_can_float(canFloat);
	
	cubePB.SerializeToOstream(&stream);
}

void CubeEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::CubeEntity cubePB;
	cubePB.ParseFromIstream(&stream);
	
	m_physicsObject.position = DeserializePos(cubePB);
	m_physicsObject.rotation = glm::quat(cubePB.rotationw(), cubePB.rotationx(), cubePB.rotationy(), cubePB.rotationz());
	m_physicsObject.displayPosition = m_physicsObject.position;
	canFloat = cubePB.can_float();
	m_showChangeGravityControlHint = cubePB.show_change_gravity_control_hint();
}

void CubeEnt::CollectPhysicsObjects(PhysicsEngine& physicsEngine, float dt)
{
	physicsEngine.RegisterObject(&m_physicsObject);
}

void CubeEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_physicsObject.position = newPosition;
}

int CubeEnt::EdGetIconIndex() const
{
	return -1;
}

std::span<const EditorSelectionMesh> CubeEnt::EdGetSelectionMeshes() const
{
	static EditorSelectionMesh selectionMesh;
	selectionMesh.model = canFloat ? woodCubeModel : cubeModel;
	selectionMesh.collisionMesh = canFloat ? &woodCubeSelColMesh : &cubeSelColMesh;
	selectionMesh.transform = GetEditorWorldMatrix();
	return { &selectionMesh, 1 };
}
