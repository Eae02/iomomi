#include "ForceFieldEnt.hpp"
#include "../EntityManager.hpp"
#include "../../WorldUpdateArgs.hpp"
#include "../../../Editor/PrimitiveRenderer.hpp"
#include "../../../Graphics/Materials/ForceFieldMaterial.hpp"
#include "../../../Graphics/RenderSettings.hpp"
#include "../../../../Protobuf/Build/ForceFieldEntity.pb.h"

#include <imgui.h>

static ForceFieldMaterial* forceFieldMaterial;
static eg::Buffer* forceFieldQuadBuffer;

static constexpr float PARTICLE_GRAVITY = 4.0f;
static constexpr float PARTICLE_WIDTH = 0.02f;
static constexpr float PARTICLE_EMIT_DELAY = 0.025f;
static float particleHeight;

static void OnInit()
{
	forceFieldMaterial = new ForceFieldMaterial;
	
	const float vertices[] = { -1, -1, -1, 1, 1, -1, 1, 1 };
	forceFieldQuadBuffer = new eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(vertices), vertices);
	forceFieldQuadBuffer->UsageHint(eg::BufferUsage::VertexBuffer);
	
	const eg::Texture& particleTex = eg::GetAsset<eg::Texture>("Textures/ForceFieldParticle.png");
	particleHeight = particleTex.Height() * PARTICLE_WIDTH / particleTex.Width();
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
}

static inline glm::vec4 MakeTransformVectorWithLen(const glm::vec3& v)
{
	return glm::vec4(v, glm::length(v));
}

void ForceFieldEnt::Draw(const EntDrawArgs& args)
{
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
	
	const float radDown = radius[(int)newGravity / 2];
	const glm::vec3 down = glm::vec3(DirectionVector(newGravity));
	const glm::vec3& cameraPos = RenderSettings::instance->cameraPosition;
	
	for (const ForceFieldParticle& particle : particles)
	{
		float trailTime = std::max(particle.elapsedTime - 0.1f, 0.0f);
		float s1 = PARTICLE_GRAVITY * trailTime * trailTime;
		float s2 = PARTICLE_GRAVITY * particle.elapsedTime * particle.elapsedTime;
		glm::vec3 pos1 = particle.start + down * glm::clamp(s1, 0.0f, radDown * 2);
		glm::vec3 pos2 = particle.start + down * glm::clamp(s2, 0.0f, radDown * 2);
		
		glm::vec3 center = (pos1 + pos2) / 2.0f;
		glm::vec3 toCamera = glm::normalize(cameraPos - center);
		
		ForceFieldInstance instance;
		instance.mode = FFMode::Particle;
		instance.offset = center;
		instance.transformX = MakeTransformVectorWithLen(glm::normalize(glm::cross(down, toCamera)) * PARTICLE_WIDTH * 0.5f);
		instance.transformY = MakeTransformVectorWithLen(pos2 - center);
		
		args.transparentMeshBatch->Add(mesh, *forceFieldMaterial, instance, DepthDrawOrder(instance.offset));
	}
}

static std::mt19937 particleRNG;

void ForceFieldEnt::Update(const WorldUpdateArgs& args)
{
	const float radDown = radius[(int)newGravity / 2];
	const float maxS = radDown * 2 + 1.0f;
	
	for (int64_t i = particles.size() - 1; i >= 0; i--)
	{
		particles[i].elapsedTime += args.dt;
		float s = PARTICLE_GRAVITY * particles[i].elapsedTime * particles[i].elapsedTime;
		if (s > maxS)
		{
			particles[i] = particles.back();
			particles.pop_back();
		}
	}
	
	glm::vec3 centerStart = m_position - glm::vec3(DirectionVector(newGravity)) * radDown;
	
	int tangentDir = (((int)newGravity / 2) + 1) % 3;
	int bitangentDir = (tangentDir + 1) % 3;
	
	int sectionsT = std::ceil(radius[tangentDir] * 2);
	int sectionsB = std::ceil(radius[bitangentDir] * 2);
	
	glm::vec3 tangent, bitangent;
	tangent[tangentDir] = radius[tangentDir];
	bitangent[bitangentDir] = radius[bitangentDir];
	
	timeSinceEmission += args.dt;
	while (timeSinceEmission > PARTICLE_EMIT_DELAY)
	{
		for (int x = 0; x < sectionsT; x++)
		{
			for (int y = 0; y < sectionsB; y++)
			{
				float t = (std::uniform_real_distribution<float>(0.0f, 1.0f)(particleRNG) + x) / sectionsT;
				float u = (std::uniform_real_distribution<float>(0.0f, 1.0f)(particleRNG) + y) / sectionsB;
				
				ForceFieldParticle& particle = particles.emplace_back();
				particle.elapsedTime = 0;
				particle.start = centerStart + tangent * (t * 2 - 1) + bitangent * (u * 2 - 1);
			}
		}
		
		timeSinceEmission -= PARTICLE_EMIT_DELAY;
	}
}

const glm::ivec3 cubeVertices[] = 
{
	{ -1, -1, -1 },
	{ -1,  1, -1 },
	{ -1, -1,  1 },
	{ -1,  1,  1 },
	{  1, -1, -1 },
	{  1,  1, -1 },
	{  1, -1,  1 },
	{  1,  1,  1 },
};

const int cubeFaces[6][4] =
{
	{ 0, 1, 2, 3 },
	{ 4, 5, 6, 7 },
	{ 0, 2, 4, 6 },
	{ 1, 3, 5, 7 },
	{ 0, 1, 4, 5 },
	{ 2, 3, 6, 7 }
};

const std::pair<int, int> cubeEdges[] = 
{
	{ 0, 4 },
	{ 1, 5 },
	{ 2, 6 },
	{ 3, 7 },
	{ 0, 1 },
	{ 0, 2 },
	{ 1, 3 },
	{ 2, 3 },
	{ 4, 5 },
	{ 4, 6 },
	{ 5, 7 },
	{ 6, 7 }
};

void ForceFieldEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	glm::vec3 realVertices[8];
	for (int i = 0; i < 8; i++)
	{
		realVertices[i] = m_position + radius * glm::vec3(cubeVertices[i]);
	}
	
	for (int f = 0; f < 6; f++)
	{
		glm::vec3 positions[4];
		for (int i = 0; i < 4; i++)
			positions[i] = realVertices[cubeFaces[f][i]];
		args.primitiveRenderer->AddQuad(positions, eg::ColorSRGB::FromRGBAHex(0x2390c333));
	}
	
	for (std::pair<int, int> edge : cubeEdges)
	{
		args.primitiveRenderer->AddLine(realVertices[edge.first], realVertices[edge.second], eg::ColorSRGB::FromRGBAHex(0x2390c3BB));
	}
}

std::optional<Dir> ForceFieldEnt::CheckIntersection(EntityManager& entityManager, const eg::AABB& aabb)
{
	std::optional<Dir> result;
	entityManager.ForEachOfType<ForceFieldEnt>([&] (const ForceFieldEnt& entity)
	{
		if (aabb.Intersects(eg::AABB(entity.m_position - entity.radius, entity.m_position + entity.radius)))
			result = entity.newGravity;
	});
	return result;
}

void ForceFieldEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::ForceFieldEntity forceFieldPB;
	
	SerializePos(forceFieldPB);
	
	forceFieldPB.set_radx(radius.x);
	forceFieldPB.set_rady(radius.y);
	forceFieldPB.set_radz(radius.z);
	
	forceFieldPB.set_new_gravity((gravity_pb::Dir)newGravity);
	
	forceFieldPB.SerializeToOstream(&stream);
}

void ForceFieldEnt::Deserialize(std::istream& stream)
{
	gravity_pb::ForceFieldEntity forceFieldPB;
	forceFieldPB.ParseFromIstream(&stream);
	
	DeserializePos(forceFieldPB);
	
	radius = glm::vec3(forceFieldPB.radx(), forceFieldPB.rady(), forceFieldPB.radz());
	newGravity = (Dir)forceFieldPB.new_gravity();
}
