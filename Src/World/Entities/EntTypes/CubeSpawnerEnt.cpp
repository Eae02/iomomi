#include "CubeSpawnerEnt.hpp"
#include "CubeEnt.hpp"
#include "../../World.hpp"
#include "../../WorldUpdateArgs.hpp"
#include "../../../../Protobuf/Build/CubeSpawnerEntity.pb.h"
#include <imgui.h>

void CubeSpawnerEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::Checkbox("Float", &m_cubeCanFloat);
	ImGui::Checkbox("Autospawn", &m_spawnIfNoCube);
}

void CubeSpawnerEnt::Update(const WorldUpdateArgs& args)
{
	if (args.player == nullptr)
		return;
	
	bool activated = m_activatable.AllSourcesActive();
	
	std::shared_ptr<Ent> cube = m_cube.lock();
	
	bool spawnNew = false;
	if (cube == nullptr && m_spawnIfNoCube)
	{
		spawnNew = true;
	}
	else if (activated && !m_wasActivated)
	{
		if (cube)
			args.world->entManager.RemoveEntity(*cube);
		spawnNew = true;
	}
	
	if (spawnNew)
	{
		glm::vec3 spawnDir(DirectionVector(m_direction));
		glm::vec3 spawnPos = m_position + spawnDir * (CubeEnt::RADIUS + 0.01f);
		
		cube = Ent::Create<CubeEnt>(spawnPos, m_cubeCanFloat);
		args.world->InitRigidBodyEntity(*cube);
		m_cube = cube;
		args.world->entManager.AddEntity(std::move(cube));
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
	gravity_pb::CubeSpawnerEntity cubeSpawnerPB;
	
	SerializePos(cubeSpawnerPB);
	cubeSpawnerPB.set_dir((gravity_pb::Dir)m_direction);
	
	cubeSpawnerPB.set_cube_can_float(m_cubeCanFloat);
	cubeSpawnerPB.set_spawn_if_none(m_spawnIfNoCube);
	
	cubeSpawnerPB.set_name(m_activatable.m_name);
	
	cubeSpawnerPB.SerializeToOstream(&stream);
}

void CubeSpawnerEnt::Deserialize(std::istream& stream)
{
	gravity_pb::CubeSpawnerEntity cubeSpawnerPB;
	cubeSpawnerPB.ParseFromIstream(&stream);
	
	DeserializePos(cubeSpawnerPB);
	m_direction = (Dir)cubeSpawnerPB.dir();
	m_cubeCanFloat = cubeSpawnerPB.cube_can_float();
	m_spawnIfNoCube = cubeSpawnerPB.spawn_if_none();
	
	if (cubeSpawnerPB.name() != 0)
		m_activatable.m_name = cubeSpawnerPB.name();
}
