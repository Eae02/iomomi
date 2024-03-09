#include "CubeSpawnerEnt.hpp"

#include "../../../../../Protobuf/Build/CubeSpawnerEntity.pb.h"
#include "../../../../Graphics/Lighting/PointLightShadowMapper.hpp"
#include "../../../../Graphics/Materials/EmissiveMaterial.hpp"
#include "../../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../ImGui.hpp"
#include "CubeEnt.hpp"

DEF_ENT_TYPE(CubeSpawnerEnt)

static const eg::Model* cubeSpawnerModel;
static int lightsMaterialIndex;
static const eg::IMaterial* cubeSpawnerMaterial;

static size_t door1MeshIndex;
static size_t door2MeshIndex;

static void OnInit()
{
	cubeSpawnerModel = &eg::GetAsset<eg::Model>("Models/CubeSpawner.aa.obj");
	lightsMaterialIndex = cubeSpawnerModel->GetMaterialIndex("Lights");
	cubeSpawnerMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/CubeSpawner.yaml");
	door1MeshIndex = cubeSpawnerModel->GetMeshIndex("Door1");
	door2MeshIndex = cubeSpawnerModel->GetMeshIndex("Door2");
}

EG_ON_INIT(OnInit)

void CubeSpawnerEnt::RenderSettings()
{
#ifdef EG_HAS_IMGUI
	Ent::RenderSettings();

	ImGui::Checkbox("Float", &m_cubeCanFloat);

	ImGui::Checkbox("Require Activation", &m_requireActivation);
	ImGui::Checkbox("Reset on Activation", &m_activationResets);
#endif
}

static const eg::ColorSRGB emissiveColor = eg::ColorSRGB::FromHex(0xD1F8FE);
static constexpr float EMISSIVE_STRENGTH = 3.0f;

CubeSpawnerEnt::CubeSpawnerEnt() : m_activatable(&CubeSpawnerEnt::GetConnectionPoints) {}

std::vector<glm::vec3> CubeSpawnerEnt::GetConnectionPoints(const Ent& entity)
{
	const CubeSpawnerEnt& spawnerEnt = static_cast<const CubeSpawnerEnt&>(entity);

	const glm::mat3 rotationScale = CubeEnt::RADIUS * GetRotationMatrix(spawnerEnt.m_direction);

	std::vector<glm::vec3> points;
	points.push_back(spawnerEnt.m_position + rotationScale * glm::vec3(1, 0, 0));
	points.push_back(spawnerEnt.m_position + rotationScale * glm::vec3(-1, 0, 0));
	points.push_back(spawnerEnt.m_position + rotationScale * glm::vec3(0, 0, 1));
	points.push_back(spawnerEnt.m_position + rotationScale * glm::vec3(0, 0, -1));
	return points;
}

void CubeSpawnerEnt::CommonDraw(const EntDrawArgs& args)
{
	glm::mat4 transform = glm::translate(glm::mat4(1), m_position) * glm::mat4(GetRotationMatrix(m_direction)) *
	                      glm::rotate(glm::mat4(1), eg::HALF_PI, glm::vec3(0, 0, 1)) *
	                      glm::rotate(glm::mat4(1), eg::PI, glm::vec3(1, 0, 0)) *
	                      glm::scale(glm::mat4(1), glm::vec3(CubeEnt::RADIUS));

	const eg::ColorLin emissiveLin(emissiveColor);
	const glm::vec4 emissiveV4 =
		glm::vec4(emissiveLin.r, emissiveLin.g, emissiveLin.b, emissiveLin.a) * EMISSIVE_STRENGTH;

	float openProgressSmooth = glm::smoothstep(0.0f, 1.0f, m_doorOpenProgress);

	for (size_t m = 0; m < cubeSpawnerModel->NumMeshes(); m++)
	{
		if (cubeSpawnerModel->GetMesh(m).materialIndex == lightsMaterialIndex)
		{
			args.meshBatch->AddModelMesh(
				*cubeSpawnerModel, m, EmissiveMaterial::instance,
				EmissiveMaterial::InstanceData{ transform, emissiveV4 }
			);
		}
		else
		{
			glm::mat4 transformHere = transform;
			if (m == door1MeshIndex)
			{
				transformHere = glm::translate(transform, glm::vec3(0, 0, -openProgressSmooth));
			}
			else if (m == door2MeshIndex)
			{
				transformHere = glm::translate(transform, glm::vec3(0, 0, openProgressSmooth));
			}

			args.meshBatch->AddModelMesh(
				*cubeSpawnerModel, m, *cubeSpawnerMaterial, StaticPropMaterial::InstanceData(transformHere)
			);
		}
	}
}

static constexpr float DOOR_OPEN_SPEED = 0.5f;
static constexpr float DOOR_CLOSE_SPEED = 0.5f;
static constexpr float SPAWN_DIST_INSIDE = 0.1f;

void CubeSpawnerEnt::Update(const WorldUpdateArgs& args)
{
	if (args.mode != WorldMode::Game)
		return;

	bool activated = m_activatable.AllSourcesActive();

	std::shared_ptr<CubeEnt> cube = m_cube.lock();

	if (m_state == State::Initial)
	{
		bool spawnNew = false;
		if (cube == nullptr && (!m_requireActivation || m_activatable.m_enabledConnections == 0))
		{
			spawnNew = true;
		}
		else if (activated && !m_wasActivated && (!cube || m_activationResets))
		{
			if (cube)
			{
				args.world->entManager.RemoveEntity(*cube);
				cube = nullptr;
			}
			spawnNew = true;
		}
		if (spawnNew)
		{
			const glm::vec3 spawnPos =
				m_position - glm::vec3(DirectionVector(m_direction)) * (CubeEnt::RADIUS + SPAWN_DIST_INSIDE);

			cube = Ent::Create<CubeEnt>(spawnPos, m_cubeCanFloat);
			cube->isSpawning = true;
			m_cube = cube;
			args.world->entManager.AddEntity(std::move(cube));
			m_state = State::OpeningDoor;
		}
	}
	else if (m_state == State::PushingCube)
	{
		if (cube == nullptr)
		{
			m_state = State::ClosingDoor;
		}
		else
		{
			glm::vec3 toCube = cube->GetPosition() - m_position;
			if (glm::dot(toCube, glm::vec3(DirectionVector(m_direction))) >= CubeEnt::RADIUS + 0.05f)
			{
				cube->isSpawning = false;
				m_state = State::ClosingDoor;
			}
			else
			{
				constexpr float PUSH_DURATION = 0.5f;
				constexpr float PUSH_VEL = (CubeEnt::RADIUS * 2 + SPAWN_DIST_INSIDE) / PUSH_DURATION;
				cube->m_physicsObject.velocity = PUSH_VEL * glm::vec3(DirectionVector(m_direction));
			}
		}
	}
	else if (m_state == State::OpeningDoor)
	{
		m_doorOpenProgress += args.dt / DOOR_OPEN_SPEED;
		if (m_doorOpenProgress >= 1)
		{
			m_doorOpenProgress = 1;
			m_state = State::PushingCube;
		}
	}
	else if (m_state == State::ClosingDoor)
	{
		m_doorOpenProgress -= args.dt / DOOR_CLOSE_SPEED;
		if (m_doorOpenProgress <= 0)
		{
			m_doorOpenProgress = 0;
			m_state = State::Initial;
		}
	}

	m_wasActivated = activated;
}

const void* CubeSpawnerEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatableComp))
		return &m_activatable;
	return Ent::GetComponent(type);
}

void CubeSpawnerEnt::Serialize(std::ostream& stream) const
{
	iomomi_pb::CubeSpawnerEntity cubeSpawnerPB;

	SerializePos(cubeSpawnerPB, m_position);
	cubeSpawnerPB.set_dir(static_cast<iomomi_pb::Dir>(m_direction));

	cubeSpawnerPB.set_cube_can_float(m_cubeCanFloat);
	cubeSpawnerPB.set_spawn_if_none(!m_requireActivation);
	cubeSpawnerPB.set_activation_resets(m_activationResets);

	cubeSpawnerPB.set_name(m_activatable.m_name);

	cubeSpawnerPB.SerializeToOstream(&stream);
}

void CubeSpawnerEnt::Deserialize(std::istream& stream)
{
	iomomi_pb::CubeSpawnerEntity cubeSpawnerPB;
	cubeSpawnerPB.ParseFromIstream(&stream);

	m_position = DeserializePos(cubeSpawnerPB);
	m_direction = static_cast<Dir>(cubeSpawnerPB.dir());
	m_cubeCanFloat = cubeSpawnerPB.cube_can_float();
	m_requireActivation = !cubeSpawnerPB.spawn_if_none();
	m_activationResets = cubeSpawnerPB.activation_resets();

	if (cubeSpawnerPB.name() != 0)
		m_activatable.m_name = cubeSpawnerPB.name();
}

void CubeSpawnerEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
	if (faceDirection)
		m_direction = *faceDirection;
}

int CubeSpawnerEnt::EdGetIconIndex() const
{
	return 12;
}
