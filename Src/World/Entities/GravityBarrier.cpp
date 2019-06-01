#include "GravityBarrier.hpp"
#include "ECEditorVisible.hpp"
#include "Messages.hpp"
#include "../Clipping.hpp"
#include "../../Graphics/Materials/MeshDrawArgs.hpp"
#include "../../Graphics/RenderSettings.hpp"
#include "../../../Protobuf/Build/GravityBarrierEntity.pb.h"

#include <imgui.h>

struct InstanceData
{
	glm::vec3 position;
	glm::vec3 tangent;
	float tangentMag;
	glm::vec3 bitangent;
	float bitangentMag;
};

struct GravityBarrierMaterial : eg::IMaterial
{
	GravityBarrierMaterial()
	{
		eg::GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier.vs.glsl").DefaultVariant();
		pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier.fs.glsl").DefaultVariant();
		pipelineCI.vertexBindings[0] = { 2, eg::InputRate::Vertex };
		pipelineCI.vertexBindings[1] = { sizeof(InstanceData), eg::InputRate::Instance };
		pipelineCI.vertexAttributes[0] = { 0, eg::DataType::UInt8Norm, 2, 0 };
		pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 3, offsetof(InstanceData, position) };
		pipelineCI.vertexAttributes[2] = { 1, eg::DataType::Float32, 4, offsetof(InstanceData, tangent) };
		pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(InstanceData, bitangent) };
		pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
		pipelineCI.topology = eg::Topology::TriangleStrip;
		pipelineCI.cullMode = eg::CullMode::None;
		pipelineCI.enableDepthTest = true;
		pipelineCI.enableDepthWrite = false;
		pipelineCI.depthCompare = eg::CompareOp::Less;
		pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
		
		m_pipeline = eg::Pipeline::Create(pipelineCI);
		
		m_descriptorSet = eg::DescriptorSet(m_pipeline, 0);
		m_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		m_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/LineNoise.png"), 1);
	}
	
	size_t PipelineHash() const override
	{
		return typeid(GravityBarrierMaterial).hash_code();
	}
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override
	{
		MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);
		if (mDrawArgs->drawMode != MeshDrawMode::Emissive && mDrawArgs->drawMode != MeshDrawMode::Editor)
			return false;
		
		cmdCtx.BindPipeline(m_pipeline);
		cmdCtx.BindDescriptorSet(m_descriptorSet, 0);
		
		float baseIntensity;
		if (mDrawArgs->drawMode == MeshDrawMode::Editor)
		{
			baseIntensity = 5.0f;
		}
		else
		{
			baseIntensity = 0.0f;
		}
		cmdCtx.PushConstants(0, sizeof(float), &baseIntensity);
		
		return true;
	}
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override
	{
		return true;
	}
	
	eg::DescriptorSet m_descriptorSet;
	eg::Pipeline m_pipeline;
};

static GravityBarrierMaterial* material;
static eg::Buffer vertexBuffer;
static eg::MeshBatch::Mesh barrierMesh;
static eg::CollisionMesh barrierCollisionMesh;

static void OnInit()
{
	material = new GravityBarrierMaterial;
	
	uint8_t vertices[] = 
	{
		0, 0, 255, 0, 0, 255, 255, 255
	};
	vertexBuffer = eg::Buffer(eg::BufferFlags::VertexBuffer, sizeof(vertices), vertices);
	vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	
	barrierMesh.firstIndex = 0;
	barrierMesh.firstVertex = 0;
	barrierMesh.numElements = 4;
	barrierMesh.vertexBuffer = vertexBuffer;
	
	glm::vec3 colVertices[] = 
	{
		glm::vec3(-0.5f, -0.5f, 0.05f),
		glm::vec3( 0.5f, -0.5f, 0.05f),
		glm::vec3(-0.5f,  0.5f, 0.05f),
		glm::vec3( 0.5f,  0.5f, 0.05f),
		glm::vec3(-0.5f, -0.5f, -0.05f),
		glm::vec3( 0.5f, -0.5f, -0.05f),
		glm::vec3(-0.5f,  0.5f, -0.05f),
		glm::vec3( 0.5f,  0.5f, -0.05f)
	};
	
	uint32_t colIndices[] = { 0, 1, 2, 1, 3, 2, 4, 6, 5, 6, 7, 5 };
	
	barrierCollisionMesh = eg::CollisionMesh::CreateV3<uint32_t>(colVertices, colIndices);
}

static void OnShutdown()
{
	delete material;
	vertexBuffer.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

eg::EntitySignature ECGravityBarrier::EntitySignature = eg::EntitySignature::Create<
    ECGravityBarrier, ECEditorVisible, eg::ECPosition3D>();

eg::MessageReceiver ECGravityBarrier::MessageReceiver = eg::MessageReceiver::Create<ECGravityBarrier,
    DrawMessage, EditorDrawMessage, EditorRenderImGuiMessage, CalculateCollisionMessage>();

eg::Entity* ECGravityBarrier::CreateEntity(eg::EntityManager& entityManager)
{
	eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
	
	entity.InitComponent<ECEditorVisible>("Gravity Barrier");
	
	return &entity;
}

static constexpr float X_SCALE = 0.25f;

void ECGravityBarrier::Draw(eg::Entity& entity, eg::MeshBatchOrdered& meshBatch) const
{
	auto [tangent, bitangent] = GetTangents();
	
	InstanceData instanceData;
	instanceData.position = entity.GetComponent<eg::ECPosition3D>().position;
	instanceData.tangent = tangent;
	instanceData.bitangent = bitangent;
	instanceData.tangentMag = glm::length(tangent);
	instanceData.bitangentMag = glm::length(bitangent) * X_SCALE;
	
	float dist = -glm::distance2(RenderSettings::instance->cameraPosition, instanceData.position);
	meshBatch.Add(barrierMesh, *material, instanceData, dist);
}

std::tuple<glm::vec3, glm::vec3> ECGravityBarrier::GetTangents() const
{
	glm::vec3 upAxis(0);
	upAxis[upPlane] = 1;
	glm::mat3 flowRotation = glm::rotate(glm::mat4(1), flowDirection * eg::HALF_PI, upAxis);
	
	glm::vec3 size3(1);
	size3[(upPlane + 1) % 3] = size.x;
	size3[(upPlane + 2) % 3] = size.y;
	
	glm::vec3 tangent(0);
	glm::vec3 bitangent(0);
	
	tangent[(upPlane + 1) % 3] = 1;
	bitangent[(upPlane + 2) % 3] = 1;
	
	tangent = (flowRotation * tangent) * size3;
	bitangent = (flowRotation * bitangent) * size3;
	
	return std::make_tuple(tangent, bitangent);
}

void ECGravityBarrier::HandleMessage(eg::Entity& entity, const DrawMessage& message)
{
	Draw(entity, *message.transparentMeshBatch);
}

void ECGravityBarrier::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
{
	Draw(entity, *message.transparentMeshBatch);
}

void ECGravityBarrier::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
{
	ECEditorVisible::RenderDefaultSettings(entity);
	
	ImGui::DragFloat2("Size", &size.x, 0.5f);
	
	const char* planeNames[] = { "Plane X", "Plane Y", "Plane Z" };
	for (int i = 0; i < 3; i++)
	{
		if (ImGui::RadioButton(planeNames[i], upPlane == i))
		{
			upPlane = i;
		}
	}
	
	int flowDir = flowDirection + 1;
	if (ImGui::SliderInt("Flow Direction", &flowDir, 1, 4))
	{
		flowDirection = glm::clamp(flowDir, 1, 4) - 1;
	}
}

void ECGravityBarrier::HandleMessage(eg::Entity& entity, const CalculateCollisionMessage& message)
{
	auto [tangent, bitangent] = GetTangents();
	if (std::abs(glm::dot(tangent, glm::vec3(DirectionVector(message.currentDown)))) < 0.001f)
		return;
	
	glm::mat4 meshTransform = glm::mat4(
		glm::vec4(tangent, 0),
		glm::vec4(bitangent, 0),
		glm::vec4(glm::normalize(glm::cross(tangent, bitangent)), 0),
		glm::vec4(entity.GetComponent<eg::ECPosition3D>().position, 1));
	
	eg::CheckEllipsoidMeshCollision(message.clippingArgs->collisionInfo, message.clippingArgs->ellipsoid,
		message.clippingArgs->move, barrierCollisionMesh, meshTransform);
}

struct GravityBarrierSerializer : eg::IEntitySerializer
{
	std::string_view GetName() const override
	{
		return "GravityBarrier";
	}
	
	void Serialize(const eg::Entity& entity, std::ostream& stream) const override
	{
		gravity_pb::GravityBarrierEntity gravBarrierPB;
		
		glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
		gravBarrierPB.set_posx(pos.x);
		gravBarrierPB.set_posy(pos.y);
		gravBarrierPB.set_posz(pos.z);
		
		const ECGravityBarrier& barrier = entity.GetComponent<ECGravityBarrier>();
		gravBarrierPB.set_flowdirection(barrier.flowDirection);
		gravBarrierPB.set_upplane(barrier.upPlane);
		gravBarrierPB.set_sizex(barrier.size.x);
		gravBarrierPB.set_sizey(barrier.size.y);
		
		gravBarrierPB.SerializeToOstream(&stream);
	}
	
	void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
	{
		eg::Entity& entity = *ECGravityBarrier::CreateEntity(entityManager);
		
		gravity_pb::GravityBarrierEntity gravBarrierPB;
		gravBarrierPB.ParseFromIstream(&stream);
		
		entity.InitComponent<eg::ECPosition3D>(gravBarrierPB.posx(), gravBarrierPB.posy(), gravBarrierPB.posz());
		
		ECGravityBarrier& barrier = entity.GetComponent<ECGravityBarrier>();
		barrier.flowDirection = gravBarrierPB.flowdirection();
		barrier.upPlane = gravBarrierPB.upplane();
		barrier.size = glm::vec2(gravBarrierPB.sizex(), gravBarrierPB.sizey());
	}
};

eg::IEntitySerializer* ECGravityBarrier::EntitySerializer = new GravityBarrierSerializer;
