#include "ForceField.hpp"
#include "ECEditorVisible.hpp"
#include "Messages.hpp"
#include "../../Graphics/RenderSettings.hpp"
#include "../../Editor/PrimitiveRenderer.hpp"
#include "../../../Protobuf/Build/ForceFieldEntity.pb.h"

#include <imgui.h>

enum class FFMode : uint32_t
{
	Box = 0,
	Particle = 1
};

static inline glm::vec4 MakeTransformVectorWithLen(const glm::vec3& v)
{
	return glm::vec4(v, glm::length(v));
}

struct ForceFieldInstance
{
	FFMode mode;
	glm::vec3 offset;
	glm::vec4 transformX;
	glm::vec4 transformY;
};

struct ForceFieldMaterial : eg::IMaterial
{
	ForceFieldMaterial()
	{
		eg::GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.vs.glsl").DefaultVariant();
		pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/ForceField.fs.glsl").DefaultVariant();
		pipelineCI.vertexBindings[0] = { sizeof(float) * 2, eg::InputRate::Vertex };
		pipelineCI.vertexBindings[1] = { sizeof(ForceFieldInstance), eg::InputRate::Instance };
		pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 2, 0 };
		pipelineCI.vertexAttributes[1] = { 1, eg::DataType::UInt32, 1, offsetof(ForceFieldInstance, mode) };
		pipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 3, offsetof(ForceFieldInstance, offset) };
		pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(ForceFieldInstance, transformX) };
		pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(ForceFieldInstance, transformY) };
		pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
		pipelineCI.topology = eg::Topology::TriangleStrip;
		pipelineCI.cullMode = eg::CullMode::None;
		pipelineCI.enableDepthTest = true;
		pipelineCI.enableDepthWrite = false;
		pipelineCI.depthCompare = eg::CompareOp::Less;
		pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
		m_pipeline = eg::Pipeline::Create(pipelineCI);
		
		eg::SamplerDescription particleSamplerDesc;
		particleSamplerDesc.wrapU = eg::WrapMode::ClampToEdge;
		particleSamplerDesc.wrapV = eg::WrapMode::ClampToEdge;
		particleSamplerDesc.wrapW = eg::WrapMode::ClampToEdge;
		m_particleSampler = eg::Sampler(particleSamplerDesc);
		
		m_descriptorSet = eg::DescriptorSet(m_pipeline, 0);
		m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		m_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/ForceFieldParticle.png"), 1, &m_particleSampler);
	}
	
	size_t PipelineHash() const override
	{
		return typeid(ForceFieldMaterial).hash_code();
	}
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override
	{
		cmdCtx.BindPipeline(m_pipeline);
		
		cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
		
		return true;
	}
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override
	{
		return true;
	}
	
	eg::Buffer m_meshBuffer;
	eg::Pipeline m_pipeline;
	eg::Sampler m_particleSampler;
	eg::DescriptorSet m_descriptorSet;
};

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

eg::EntitySignature ECForceField::EntitySignature = eg::EntitySignature::Create<
	ECForceField,
	ECEditorVisible,
	eg::ECPosition3D
>();

eg::MessageReceiver ECForceField::MessageReceiver = eg::MessageReceiver::Create<ECForceField,
    DrawMessage, EditorRenderImGuiMessage, EditorDrawMessage>();

eg::Entity* ECForceField::CreateEntity(eg::EntityManager& entityManager)
{
	eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
	
	entity.InitComponent<ECEditorVisible>("Force Field");
	
	return &entity;
}

void ECForceField::HandleMessage(eg::Entity& entity, const DrawMessage& message)
{
	eg::MeshBatch::Mesh mesh = { };
	mesh.vertexBuffer = *forceFieldQuadBuffer;
	mesh.numElements = 4;
	
	glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
	
	glm::vec3 smallerRad = radius * 0.99f;
	
	for (int d = 0; d < 6; d++)
	{
		ForceFieldInstance instance;
		instance.mode = FFMode::Box;
		instance.offset = pos + glm::vec3(DirectionVector((Dir)d)) * smallerRad;
		instance.transformX = MakeTransformVectorWithLen(glm::vec3(voxel::tangents[d]) * smallerRad);
		instance.transformY = MakeTransformVectorWithLen(glm::vec3(voxel::biTangents[d]) * smallerRad);
		
		message.transparentMeshBatch->Add(mesh, *forceFieldMaterial, instance, DepthDrawOrder(instance.offset));
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
		
		message.transparentMeshBatch->Add(mesh, *forceFieldMaterial, instance, DepthDrawOrder(instance.offset));
	}
}

static std::mt19937 particleRNG;

void ECForceField::Update(float dt, eg::EntityManager& entityManager)
{
	for (eg::Entity& entity : entityManager.GetEntitySet(EntitySignature))
	{
		ECForceField& forceField = entity.GetComponent<ECForceField>();
		glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
		
		const float radDown = forceField.radius[(int)forceField.newGravity / 2];
		const float maxS = radDown * 2 + 1.0f;
		
		for (int64_t i = forceField.particles.size() - 1; i >= 0; i--)
		{
			forceField.particles[i].elapsedTime += dt;
			float s = PARTICLE_GRAVITY * forceField.particles[i].elapsedTime * forceField.particles[i].elapsedTime;
			if (s > maxS)
			{
				forceField.particles[i] = forceField.particles.back();
				forceField.particles.pop_back();
			}
		}
		
		glm::vec3 centerStart = pos - glm::vec3(DirectionVector(forceField.newGravity)) * radDown;
		
		int tangentDir = (((int)forceField.newGravity / 2) + 1) % 3;
		int bitangentDir = (tangentDir + 1) % 3;
		
		int sectionsT = std::ceil(forceField.radius[tangentDir] * 2);
		int sectionsB = std::ceil(forceField.radius[bitangentDir] * 2);
		
		glm::vec3 tangent, bitangent;
		tangent[tangentDir] = forceField.radius[tangentDir];
		bitangent[bitangentDir] = forceField.radius[bitangentDir];
		
		forceField.timeSinceEmission += dt;
		while (forceField.timeSinceEmission > PARTICLE_EMIT_DELAY)
		{
			for (int x = 0; x < sectionsT; x++)
			{
				for (int y = 0; y < sectionsB; y++)
				{
					float t = (std::uniform_real_distribution<float>(0.0f, 1.0f)(particleRNG) + x) / sectionsT;
					float u = (std::uniform_real_distribution<float>(0.0f, 1.0f)(particleRNG) + y) / sectionsB;
					
					ForceFieldParticle& particle = forceField.particles.emplace_back();
					particle.elapsedTime = 0;
					particle.start = centerStart + tangent * (t * 2 - 1) + bitangent * (u * 2 - 1);
				}
			}
			
			forceField.timeSinceEmission -= PARTICLE_EMIT_DELAY;
		}
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

void ECForceField::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
{
	glm::vec3 realVertices[8];
	for (int i = 0; i < 8; i++)
	{
		realVertices[i] = entity.GetComponent<eg::ECPosition3D>().position + radius * glm::vec3(cubeVertices[i]);
	}
	
	for (int f = 0; f < 6; f++)
	{
		glm::vec3 positions[4];
		for (int i = 0; i < 4; i++)
			positions[i] = realVertices[cubeFaces[f][i]];
		message.primitiveRenderer->AddQuad(positions, eg::ColorSRGB::FromRGBAHex(0x2390c333));
	}
	
	for (std::pair<int, int> edge : cubeEdges)
	{
		message.primitiveRenderer->AddLine(realVertices[edge.first], realVertices[edge.second], eg::ColorSRGB::FromRGBAHex(0x2390c3BB));
	}
}

void ECForceField::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
{
	ECEditorVisible::RenderDefaultSettings(entity);
	
	ImGui::DragFloat3("Radius", &radius.x, 0.1f);
	
	ImGui::Combo("Set Gravity", reinterpret_cast<int*>(&newGravity), DirectionNames, std::size(DirectionNames));
}

std::optional<Dir> ECForceField::CheckIntersection(eg::EntityManager& entityManager, const eg::AABB& aabb)
{
	for (const eg::Entity& entity : entityManager.GetEntitySet(EntitySignature))
	{
		const ECForceField& forceField = entity.GetComponent<ECForceField>();
		glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
		
		if (aabb.Intersects(eg::AABB(pos - forceField.radius, pos + forceField.radius)))
		{
			return forceField.newGravity;
		}
	}
	
	return { };
}

struct ForceFieldSerializer : eg::IEntitySerializer
{
	std::string_view GetName() const override
	{
		return "ForceField";
	}
	
	void Serialize(const eg::Entity& entity, std::ostream& stream) const override
	{
		gravity_pb::ForceFieldEntity forceFieldPB;
		
		glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
		forceFieldPB.set_posx(pos.x);
		forceFieldPB.set_posy(pos.y);
		forceFieldPB.set_posz(pos.z);
		
		const ECForceField& forceField = entity.GetComponent<ECForceField>();
		forceFieldPB.set_radx(forceField.radius.x);
		forceFieldPB.set_rady(forceField.radius.y);
		forceFieldPB.set_radz(forceField.radius.z);
		
		forceFieldPB.set_new_gravity((gravity_pb::Dir)forceField.newGravity);
		
		forceFieldPB.SerializeToOstream(&stream);
	}
	
	void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
	{
		eg::Entity& entity = *ECForceField::CreateEntity(entityManager);
		
		gravity_pb::ForceFieldEntity forceFieldPB;
		forceFieldPB.ParseFromIstream(&stream);
		
		entity.InitComponent<eg::ECPosition3D>(forceFieldPB.posx(), forceFieldPB.posy(), forceFieldPB.posz());
		
		ECForceField& forceField = entity.GetComponent<ECForceField>();
		forceField.radius = glm::vec3(forceFieldPB.radx(), forceFieldPB.rady(), forceFieldPB.radz());
		forceField.newGravity = (Dir)forceFieldPB.new_gravity();
	}
};

eg::IEntitySerializer* ECForceField::EntitySerializer = new ForceFieldSerializer;
