#include "ForceFieldEnt.hpp"
#include "../EntityManager.hpp"
#include "../../WorldUpdateArgs.hpp"
#include "../../../Editor/PrimitiveRenderer.hpp"
#include "../../../Graphics/Materials/ForceFieldMaterial.hpp"
#include "../../../Graphics/RenderSettings.hpp"
#include "../../../../Protobuf/Build/ForceFieldEntity.pb.h"
#include "../../../Graphics/GraphicsCommon.hpp"
#include "../../../Game.hpp"

#include <imgui.h>

static ForceFieldMaterial* forceFieldMaterial;
static eg::Buffer* forceFieldQuadBuffer;

static float* particleGravity = eg::TweakVarFloat("ffld_particle_grav", 4.0f, 0.0f);
static float* particleWidth = eg::TweakVarFloat("ffld_particle_width", 0.02f, 0.0f);
static float* particleEmitRate = eg::TweakVarFloat("ffld_particle_rate", 40.0f, 0.0f);

static void OnInit()
{
	forceFieldMaterial = new ForceFieldMaterial;
	
	const float vertices[] = { -1, -1, -1, 1, 1, -1, 1, 1 };
	forceFieldQuadBuffer = new eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(vertices), vertices);
	forceFieldQuadBuffer->UsageHint(eg::BufferUsage::VertexBuffer);
}

static void OnShutdown()
{
	delete forceFieldMaterial;
	delete forceFieldQuadBuffer;
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

void ForceFieldEnt::RenderSettings()
{
	Ent::RenderSettings();
	
	ImGui::DragFloat3("Radius", &radius.x, 0.1f);
	
	ImGui::Combo("Set Gravity", reinterpret_cast<int*>(&newGravity), DirectionNames, std::size(DirectionNames));
	
	ImGui::Combo("Activate Action", reinterpret_cast<int*>(&activateAction), "Enable\0Disable\0Flip\0");
}

static inline glm::vec4 MakeTransformVectorWithLen(const glm::vec3& v)
{
	return glm::vec4(v, glm::length(v));
}

void ForceFieldEnt::GameDraw(const EntGameDrawArgs& args)
{
	if (!args.transparentMeshBatch)
		return;
	
	eg::MeshBatch::Mesh mesh = { };
	mesh.vertexBuffer = *forceFieldQuadBuffer;
	mesh.numElements = 4;
	
	glm::vec3 smallerRad = radius * 0.99f;
	
	for (int d = 0; d < 6; d++)
	{
		ForceFieldInstance instance;
		instance.mode = FFMode::Box;
		instance.offset = m_position + glm::vec3(DirectionVector((Dir)d)) * smallerRad;
		instance.transformX = MakeTransformVectorWithLen(glm::vec3(voxel::tangents[d]) * smallerRad);
		instance.transformY = MakeTransformVectorWithLen(glm::vec3(voxel::biTangents[d]) * smallerRad);
		
		args.transparentMeshBatch->Add(mesh, *forceFieldMaterial, instance, DepthDrawOrder(instance.offset));
	}
	
	const float radDown = radius[(int)m_effectiveNewGravity / 2];
	const glm::vec3 down = glm::vec3(DirectionVector(m_effectiveNewGravity));
	const glm::vec3& cameraPos = RenderSettings::instance->cameraPosition;
	
	for (const ForceFieldParticle& particle : m_particles)
	{
		float trailTime = std::max(particle.elapsedTime - 0.1f, 0.0f);
		float s1 = *particleGravity * trailTime * trailTime;
		float s2 = *particleGravity * particle.elapsedTime * particle.elapsedTime;
		glm::vec3 pos1 = particle.start + down * glm::clamp(s1, 0.0f, radDown * 2);
		glm::vec3 pos2 = particle.start + down * glm::clamp(s2, 0.0f, radDown * 2);
		
		glm::vec3 center = (pos1 + pos2) / 2.0f;
		glm::vec3 toCamera = glm::normalize(cameraPos - center);
		
		ForceFieldInstance instance;
		instance.mode = FFMode::Particle;
		instance.offset = center;
		instance.transformX = MakeTransformVectorWithLen(glm::normalize(glm::cross(down, toCamera)) * *particleWidth * 0.5f);
		instance.transformY = MakeTransformVectorWithLen(pos2 - center);
		
		args.transparentMeshBatch->Add(mesh, *forceFieldMaterial, instance, DepthDrawOrder(instance.offset));
	}
}

std::optional<eg::ColorSRGB> ForceFieldEnt::EdGetBoxColor(bool selected) const
{
	return eg::ColorSRGB::FromHex(0xb6fdff).ScaleAlpha(selected ? 0.3f : 0.2f);
}

void ForceFieldEnt::Update(const WorldUpdateArgs& args)
{
	if (args.mode == WorldMode::Editor)
		return;
	
	//Updates enabled and gravity direction
	Dir prevNewGravity = m_effectiveNewGravity;
	m_enabled = true;
	m_effectiveNewGravity = newGravity;
	if (m_activatable.m_enabledConnections != 0)
	{
		bool activated = m_activatable.AllSourcesActive();
		switch (activateAction)
		{
		case ActivateAction::Disable:
			m_enabled = !activated;
			break;
		case ActivateAction::Enable:
			m_enabled = activated;
			break;
		case ActivateAction::Flip:
			if (activated)
				m_effectiveNewGravity = OppositeDir(newGravity);
			break;
		}
	}
	
	if (m_effectiveNewGravity != prevNewGravity)
	{
		m_timeSinceEmission = 0;
		m_particles.clear();
	}
	
	//Simulated particles
	const float radDown = radius[(int)m_effectiveNewGravity / 2];
	const float maxS = radDown * 2 + 1.0f;
	for (int64_t i = m_particles.size() - 1; i >= 0; i--)
	{
		m_particles[i].elapsedTime += args.dt;
		float s = *particleGravity * m_particles[i].elapsedTime * m_particles[i].elapsedTime;
		if (s > maxS)
		{
			m_particles[i] = m_particles.back();
			m_particles.pop_back();
		}
	}
	
	//Emits new particles
	if (m_enabled)
	{
		glm::vec3 centerStart = m_position - glm::vec3(DirectionVector(m_effectiveNewGravity)) * radDown;
		
		int tangentDir = (((int)m_effectiveNewGravity / 2) + 1) % 3;
		int bitangentDir = (tangentDir + 1) % 3;
		
		int sectionsT = std::ceil(radius[tangentDir] * 2);
		int sectionsB = std::ceil(radius[bitangentDir] * 2);
		
		glm::vec3 tangent, bitangent;
		tangent[tangentDir] = radius[tangentDir];
		bitangent[bitangentDir] = radius[bitangentDir];
		
		const float emitDelay = 1.0f / *particleEmitRate;
		
		m_timeSinceEmission += args.dt;
		while (m_timeSinceEmission > emitDelay)
		{
			for (int x = 0; x < sectionsT; x++)
			{
				for (int y = 0; y < sectionsB; y++)
				{
					float t = (std::uniform_real_distribution<float>(0.0f, 1.0f)(globalRNG) + x) / sectionsT;
					float u = (std::uniform_real_distribution<float>(0.0f, 1.0f)(globalRNG) + y) / sectionsB;
					
					ForceFieldParticle& particle = m_particles.emplace_back();
					particle.elapsedTime = 0;
					particle.start = centerStart + tangent * (t * 2 - 1) + bitangent * (u * 2 - 1);
				}
			}
			
			m_timeSinceEmission -= emitDelay;
		}
	}
	else
	{
		m_timeSinceEmission = 0;
	}
}

std::optional<Dir> ForceFieldEnt::CheckIntersection(EntityManager& entityManager, const eg::AABB& aabb)
{
	std::optional<Dir> result;
	entityManager.ForEachOfType<ForceFieldEnt>([&] (const ForceFieldEnt& entity)
	{
		if (entity.m_enabled && aabb.Intersects(eg::AABB(entity.m_position - entity.radius, entity.m_position + entity.radius)))
			result = entity.m_effectiveNewGravity;
	});
	return result;
}

void ForceFieldEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::ForceFieldEntity forceFieldPB;
	
	SerializePos(forceFieldPB, m_position);
	
	forceFieldPB.set_radx(radius.x);
	forceFieldPB.set_rady(radius.y);
	forceFieldPB.set_radz(radius.z);
	forceFieldPB.set_new_gravity((gravity_pb::Dir)newGravity);
	forceFieldPB.set_activate_action((uint32_t)activateAction);
	forceFieldPB.set_name(m_activatable.m_name);
	
	forceFieldPB.SerializeToOstream(&stream);
}

void ForceFieldEnt::Deserialize(std::istream& stream)
{
	gravity_pb::ForceFieldEntity forceFieldPB;
	forceFieldPB.ParseFromIstream(&stream);
	
	m_position = DeserializePos(forceFieldPB);
	
	radius = glm::vec3(forceFieldPB.radx(), forceFieldPB.rady(), forceFieldPB.radz());
	newGravity = m_effectiveNewGravity = (Dir)forceFieldPB.new_gravity();
	activateAction = (ActivateAction)forceFieldPB.activate_action();
	m_enabled = true;
	
	if (forceFieldPB.name() != 0)
		m_activatable.m_name = forceFieldPB.name();
}

void ForceFieldEnt::EdMoved(const glm::vec3& newPosition, std::optional<Dir> faceDirection)
{
	m_position = newPosition;
}

glm::vec3 ForceFieldEnt::EdGetSize() const
{
	return radius;
}

void ForceFieldEnt::EdResized(const glm::vec3& newSize)
{
	radius = newSize;
}

std::vector<glm::vec3> ForceFieldEnt::GetConnectionPoints(const Ent& entity)
{
	const ForceFieldEnt& ffEnt = static_cast<const ForceFieldEnt&>(entity);
	
	std::vector<glm::vec3> points;
	for (int dir = 0; dir < 6; dir++) {
		points.push_back(ffEnt.m_position + ffEnt.radius * glm::vec3(DirectionVector((Dir)dir)));
	}
	
	return points;
}

const void* ForceFieldEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatableComp))
		return &m_activatable;
	return nullptr;
}
