#include "ForceField.hpp"
#include "ECEditorVisible.hpp"
#include "Messages.hpp"
#include "../../Graphics/RenderSettings.hpp"
#include "../../Editor/PrimitiveRenderer.hpp"
#include "../../../Protobuf/Build/ForceFieldEntity.pb.h"

#include <imgui.h>

enum class FFMode : uint32_t
{
	Box = 0
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
		//pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
		pipelineCI.topology = eg::Topology::TriangleStrip;
		pipelineCI.cullMode = eg::CullMode::None;
		pipelineCI.enableDepthTest = true;
		pipelineCI.enableDepthWrite = false;
		pipelineCI.depthCompare = eg::CompareOp::Less;
		pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
		m_pipeline = eg::Pipeline::Create(pipelineCI);
	}
	
	size_t PipelineHash() const override
	{
		return typeid(ForceFieldMaterial).hash_code();
	}
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override
	{
		cmdCtx.BindPipeline(m_pipeline);
		
		cmdCtx.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, 0, RenderSettings::BUFFER_SIZE);
		
		return true;
	}
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override
	{
		return true;
	}
	
	eg::Buffer m_meshBuffer;
	eg::Pipeline m_pipeline;
};

static ForceFieldMaterial* forceFieldMaterial;
static eg::Buffer* forceFieldQuadBuffer;

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
	
	for (int d = 0; d < 6; d++)
	{
		ForceFieldInstance instance;
		instance.mode = FFMode::Box;
		instance.offset = pos + glm::vec3(DirectionVector((Dir)d)) * radius;
		instance.transformX = MakeTransformVectorWithLen(glm::vec3(voxel::tangents[d]) * radius);
		instance.transformY = MakeTransformVectorWithLen(glm::vec3(voxel::biTangents[d]) * radius);
		
		message.transparentMeshBatch->Add(mesh, *forceFieldMaterial, instance, DepthDrawOrder(instance.offset));
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
	
	ImGui::DragFloat3("Position", &radius.x, 0.1f);
	
	ImGui::Combo("Set Gravity", reinterpret_cast<int*>(&newGravity), DirectionNames, std::size(DirectionNames));
}

std::optional<Dir> ECForceField::CheckIntersection(const eg::EntityManager& entityManager, const eg::AABB& aabb)
{
	
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
