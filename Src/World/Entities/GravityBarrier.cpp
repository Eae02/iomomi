#include "GravityBarrier.hpp"
#include "ECEditorVisible.hpp"
#include "Messages.hpp"
#include "ECRigidBody.hpp"
#include "ECActivatable.hpp"
#include "../Player.hpp"
#include "../WorldUpdateArgs.hpp"
#include "../World.hpp"
#include "../BulletPhysics.hpp"
#include "../Clipping.hpp"
#include "../../Graphics/Materials/MeshDrawArgs.hpp"
#include "../../Graphics/RenderSettings.hpp"
#include "../../../Protobuf/Build/GravityBarrierEntity.pb.h"
#include "../../Graphics/Materials/StaticPropMaterial.hpp"

#include <imgui.h>

static constexpr float OPACITY_SCALE = 0.0004;
static constexpr int NUM_INTERACTABLES = 8;

#pragma pack(push, 1)
struct BarrierBufferData
{
	uint32_t iaDownAxis[NUM_INTERACTABLES];
	glm::vec4 iaPosition[NUM_INTERACTABLES];
};

struct InstanceData
{
	glm::vec3 position;
	uint8_t opacity;
	uint8_t blockedAxis;
	uint8_t _p1;
	uint8_t _p2;
	glm::vec3 tangent;
	float tangentMag;
	glm::vec3 bitangent;
	float bitangentMag;
};
#pragma pack(pop)

struct GravityBarrierMaterial : eg::IMaterial
{
	GravityBarrierMaterial()
	{
		const auto& fs = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier.fs.glsl");
		
		eg::GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModuleAsset>("Shaders/GravityBarrier.vs.glsl").DefaultVariant();
		pipelineCI.fragmentShader = fs.GetVariant("VGame");
		pipelineCI.vertexBindings[0] = { 2, eg::InputRate::Vertex };
		pipelineCI.vertexBindings[1] = { sizeof(InstanceData), eg::InputRate::Instance };
		pipelineCI.vertexAttributes[0] = { 0, eg::DataType::UInt8Norm, 2, 0 };
		pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 3, offsetof(InstanceData, position) };
		pipelineCI.vertexAttributes[2] = { 1, eg::DataType::UInt8, 2, offsetof(InstanceData, opacity) };
		pipelineCI.vertexAttributes[3] = { 1, eg::DataType::Float32, 4, offsetof(InstanceData, tangent) };
		pipelineCI.vertexAttributes[4] = { 1, eg::DataType::Float32, 4, offsetof(InstanceData, bitangent) };
		pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
		pipelineCI.topology = eg::Topology::TriangleStrip;
		pipelineCI.cullMode = eg::CullMode::None;
		pipelineCI.enableDepthTest = true;
		pipelineCI.enableDepthWrite = false;
		pipelineCI.depthCompare = eg::CompareOp::Less;
		pipelineCI.blendStates[0] = eg::BlendState(eg::BlendFunc::Add, eg::BlendFactor::One, eg::BlendFactor::OneMinusSrcAlpha);
		
		m_pipelineGame = eg::Pipeline::Create(pipelineCI);
		
		pipelineCI.fragmentShader = fs.GetVariant("VEditor");
		m_pipelineEditor = eg::Pipeline::Create(pipelineCI);
		
		m_barrierDataBuffer = eg::Buffer(eg::BufferFlags::Update | eg::BufferFlags::UniformBuffer,
			sizeof(BarrierBufferData), nullptr);
		
		const eg::Texture& lineNoiseTex = eg::GetAsset<eg::Texture>("Textures/LineNoise.png");
		
		m_descriptorSetGame = eg::DescriptorSet(m_pipelineGame, 0);
		m_descriptorSetGame.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		m_descriptorSetGame.BindTexture(lineNoiseTex, 1);
		m_descriptorSetGame.BindUniformBuffer(m_barrierDataBuffer, 2, 0, sizeof(BarrierBufferData));
		
		m_descriptorSetEditor = eg::DescriptorSet(m_pipelineEditor, 0);
		m_descriptorSetEditor.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		m_descriptorSetEditor.BindTexture(lineNoiseTex, 1);
	}
	
	size_t PipelineHash() const override
	{
		return typeid(GravityBarrierMaterial).hash_code();
	}
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override
	{
		MeshDrawArgs* mDrawArgs = reinterpret_cast<MeshDrawArgs*>(drawArgs);
		
		if (mDrawArgs->drawMode == MeshDrawMode::Emissive)
		{
			cmdCtx.BindPipeline(m_pipelineGame);
			cmdCtx.BindDescriptorSet(m_descriptorSetGame, 0);
			return true;
		}
		if (mDrawArgs->drawMode == MeshDrawMode::Editor)
		{
			cmdCtx.BindPipeline(m_pipelineEditor);
			cmdCtx.BindDescriptorSet(m_descriptorSetEditor, 0);
			return true;
		}
		
		return false;
	}
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override
	{
		return true;
	}
	
	eg::Pipeline m_pipelineGame;
	eg::Buffer m_barrierDataBuffer;
	eg::DescriptorSet m_descriptorSetGame;
	
	eg::Pipeline m_pipelineEditor;
	eg::DescriptorSet m_descriptorSetEditor;
};

static GravityBarrierMaterial* material;
static eg::Buffer vertexBuffer;
static eg::MeshBatch::Mesh barrierMesh;
static eg::CollisionMesh barrierCollisionMesh;

static eg::Model* barrierBorderModel;

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
	
	const glm::vec3 colVertices[] = 
	{
		glm::vec3(-0.5f, -0.5f,  0.05f),
		glm::vec3( 0.5f, -0.5f,  0.05f),
		glm::vec3(-0.5f,  0.5f,  0.05f),
		glm::vec3( 0.5f,  0.5f,  0.05f),
		glm::vec3(-0.5f, -0.5f, -0.05f),
		glm::vec3( 0.5f, -0.5f, -0.05f),
		glm::vec3(-0.5f,  0.5f, -0.05f),
		glm::vec3( 0.5f,  0.5f, -0.05f)
	};
	
	const uint32_t colIndices[] = { 0, 1, 2, 1, 3, 2, 4, 6, 5, 6, 7, 5 };
	
	barrierCollisionMesh = eg::CollisionMesh::CreateV3<uint32_t>(colVertices, colIndices);
	
	barrierBorderModel = &eg::GetAsset<eg::Model>("Models/GravityBarrierBorder.obj");
}

static void OnShutdown()
{
	delete material;
	vertexBuffer.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

static const eg::EntitySignature interactableSignature = eg::EntitySignature::Create<ECBarrierInteractable, eg::ECPosition3D>();

void ECGravityBarrier::PrepareForDraw(const Player& player, eg::EntityManager& entityManager)
{
	BarrierBufferData bufferData;
	
	bufferData.iaDownAxis[0] = (int)player.CurrentDown() / 2;
	bufferData.iaPosition[0] = glm::vec4(player.Position(), 0.0f);
	
	int itemsWritten = 1;
	for (const eg::Entity& entity : entityManager.GetEntitySet(interactableSignature))
	{
		bufferData.iaDownAxis[itemsWritten] = (int)entity.GetComponent<ECBarrierInteractable>().currentDown / 2;
		bufferData.iaPosition[itemsWritten] = glm::vec4(entity.GetComponent<eg::ECPosition3D>().position, 0.0f);
		itemsWritten++;
		
		if (itemsWritten >= NUM_INTERACTABLES)
			break;
	}
	
	while (itemsWritten < NUM_INTERACTABLES)
	{
		bufferData.iaDownAxis[itemsWritten] = 3;
		bufferData.iaPosition[itemsWritten] = glm::vec4(0.0f);
		itemsWritten++;
	}
	
	eg::DC.UpdateBuffer(material->m_barrierDataBuffer, 0, sizeof(BarrierBufferData), &bufferData);
	material->m_barrierDataBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
}

eg::EntitySignature ECGravityBarrier::EntitySignature = eg::EntitySignature::Create<
    ECGravityBarrier, ECEditorVisible, ECRigidBody, ECActivatable, eg::ECPosition3D>();

eg::MessageReceiver ECGravityBarrier::MessageReceiver = eg::MessageReceiver::Create<ECGravityBarrier,
    DrawMessage, EditorDrawMessage, EditorRenderImGuiMessage, CalculateCollisionMessage>();

eg::Entity* ECGravityBarrier::CreateEntity(eg::EntityManager& entityManager)
{
	eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
	
	entity.InitComponent<ECEditorVisible>("Gravity Barrier");
	
	entity.InitComponent<ECActivatable>([] (const eg::Entity& e)
	{
		return e.GetComponent<ECGravityBarrier>().GetActivationPoints(e);
	});
	
	return &entity;
}

std::vector<glm::vec3> ECGravityBarrier::GetActivationPoints(const eg::Entity& entity) const
{
	auto [tangent, bitangent] = GetTangents();
	
	glm::vec3 position = entity.GetComponent<eg::ECPosition3D>().position;
	
	return {
		position + bitangent * 0.5f,
		position - bitangent * 0.5f,
		position + tangent * 0.5f,
		position - tangent * 0.5f
	};
}

glm::mat4 ECGravityBarrier::GetTransform(const eg::Entity& entity)
{
	auto [tangent, bitangent] = entity.GetComponent<ECGravityBarrier>().GetTangents();
	return glm::mat4(
		glm::vec4(tangent, 0),
		glm::vec4(bitangent, 0),
		glm::vec4(glm::normalize(glm::cross(tangent, bitangent)), 0),
		glm::vec4(entity.GetComponent<eg::ECPosition3D>().position, 1));
}

void ECGravityBarrier::Draw(eg::Entity& entity, eg::MeshBatchOrdered& meshBatchOrdered, eg::MeshBatch& meshBatch) const
{
	auto [tangent, bitangent] = GetTangents();
	
	float tangentLen = glm::length(tangent);
	float bitangentLen = glm::length(bitangent);
	
	constexpr float TIME_SCALE = 0.25f;
	
	InstanceData instanceData;
	instanceData.position = entity.GetComponent<eg::ECPosition3D>().position;
	instanceData.opacity = 255 * glm::smoothstep(0.0f, 1.0f, m_opacity);
	instanceData.blockedAxis = BlockedAxis();
	instanceData.tangent = tangent;
	instanceData.bitangent = bitangent;
	instanceData.tangentMag = glm::length(tangentLen);
	instanceData.bitangentMag = glm::length(bitangentLen) * TIME_SCALE;
	
	meshBatchOrdered.Add(barrierMesh, *material, instanceData, DepthDrawOrder(instanceData.position));
	
	glm::vec3 tangentNorm = tangent / tangentLen;
	glm::vec3 bitangentNorm = bitangent / bitangentLen;
	glm::vec3 tcb = glm::cross(tangentNorm, bitangentNorm);
	
	auto AddBorderModelInstance = [&] (const glm::mat4& transform)
	{
		eg::IMaterial* borderMaterial = &eg::GetAsset<StaticPropMaterial>("Materials/Default.yaml");
		for (size_t m = 0; m < barrierBorderModel->NumMeshes(); m++)
		{
			meshBatch.AddModelMesh(*barrierBorderModel, m, *borderMaterial, transform);
		}
	};
	
	int tangentLenI = (int)std::round(tangentLen);
	int bitangentLenI = (int)std::round(bitangentLen);
	
	for (int i = -tangentLenI; i < tangentLenI; i++)
	{
		glm::vec3 translation1 = instanceData.position + bitangent * 0.5f + tangentNorm * ((i + 0.5f) * 0.5f);
		AddBorderModelInstance(glm::mat4x3(tcb, -bitangentNorm, tangentNorm, translation1));
		
		glm::vec3 translation2 = instanceData.position - bitangent * 0.5f + tangentNorm * ((i + 0.5f) * 0.5f);
		AddBorderModelInstance(glm::mat4x3(-tcb, bitangentNorm, tangentNorm, translation2));
	}
	
	for (int i = -bitangentLenI; i < bitangentLenI; i++)
	{
		glm::vec3 translation1 = instanceData.position + tangent * 0.5f + bitangentNorm * ((i + 0.5f) * 0.5f);
		AddBorderModelInstance(glm::mat4x3(-tcb, -tangentNorm, bitangentNorm, translation1));
		
		glm::vec3 translation2 = instanceData.position - tangent * 0.5f + bitangentNorm * ((i + 0.5f) * 0.5f);
		AddBorderModelInstance(glm::mat4x3(tcb, tangentNorm, bitangentNorm, translation2));
	}
}

int ECGravityBarrier::BlockedAxis() const
{
	if (m_blockFalling)
		return upPlane;
	auto [tangent, bitangent] = GetTangents();
	if (std::abs(tangent.x) > 0.5f)
		return 0;
	if (std::abs(tangent.y) > 0.5f)
		return 1;
	return 2;
}

void ECGravityBarrier::Update(const WorldUpdateArgs& args)
{
	for (eg::Entity& entity : args.world->EntityManager().GetEntitySet(EntitySignature))
	{
		ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
		ECGravityBarrier& barrier = entity.GetComponent<ECGravityBarrier>();
		ECActivatable& activatable = entity.GetComponent<ECActivatable>();
		
		barrier.m_enabled = true;
		int newFlowDirectionOffset = 0;
		
		if (activatable.EnabledConnections() != 0)
		{
			bool activated = activatable.AllSourcesActive();
			if (activated && barrier.activateAction == ActivateAction::Disable)
				barrier.m_enabled = false;
			else if (!activated && barrier.activateAction == ActivateAction::Enable)
				barrier.m_enabled = false;
			else if (activated && barrier.activateAction == ActivateAction::Rotate)
				newFlowDirectionOffset = 1;
		}
		
		btBroadphaseProxy* broadphaseProxy = rigidBody.GetRigidBody()->getBroadphaseProxy();
		broadphaseProxy->m_collisionFilterMask = barrier.m_enabled ? (2 << barrier.BlockedAxis()) : 0;
		broadphaseProxy->m_collisionFilterGroup = -1;
		
		bool decOpacity = !barrier.m_enabled;
		
		if (newFlowDirectionOffset != barrier.m_flowDirectionOffset)
		{
			if (barrier.m_opacity < 0.01f)
			{
				barrier.m_flowDirectionOffset = newFlowDirectionOffset;
			}
			else
			{
				decOpacity = true;
			}
		}
		
		constexpr float OPACITY_ANIMATION_TIME = 0.25f;
		if (decOpacity)
			barrier.m_opacity = std::max(barrier.m_opacity - args.dt / OPACITY_ANIMATION_TIME, 0.0f);
		else
			barrier.m_opacity = std::min(barrier.m_opacity + args.dt / OPACITY_ANIMATION_TIME, 1.0f);
	}
}

std::tuple<glm::vec3, glm::vec3> ECGravityBarrier::GetTangents() const
{
	glm::vec3 upAxis(0);
	upAxis[upPlane] = 1;
	glm::mat3 flowRotation = glm::rotate(glm::mat4(1), (flowDirection + m_flowDirectionOffset) * eg::HALF_PI, upAxis);
	
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
	Draw(entity, *message.transparentMeshBatch, *message.meshBatch);
}

void ECGravityBarrier::HandleMessage(eg::Entity& entity, const EditorDrawMessage& message)
{
	Draw(entity, *message.transparentMeshBatch, *message.meshBatch);
}

void ECGravityBarrier::HandleMessage(eg::Entity& entity, const EditorRenderImGuiMessage& message)
{
	ECEditorVisible::RenderDefaultSettings(entity);
	
	ImGui::DragFloat2("Size", &size.x, 0.5f);
	
	ImGui::Combo("Plane", &upPlane, "X\0Y\0Z\0");
	
	ImGui::Checkbox("Block Falling", &m_blockFalling);
	
	int flowDir = flowDirection + 1;
	if (ImGui::SliderInt("Flow Direction", &flowDir, 1, 4))
	{
		flowDirection = glm::clamp(flowDir, 1, 4) - 1;
	}
	
	ImGui::Combo("Activate Action", reinterpret_cast<int*>(&activateAction), "Disable\0Enable\0Rotate\0");
}

void ECGravityBarrier::HandleMessage(eg::Entity& entity, const CalculateCollisionMessage& message)
{
	if (m_enabled && (int)message.currentDown / 2 == BlockedAxis())
	{
		eg::CheckEllipsoidMeshCollision(message.clippingArgs->collisionInfo, message.clippingArgs->ellipsoid,
		                                message.clippingArgs->move, barrierCollisionMesh, GetTransform(entity));
	}
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
		gravBarrierPB.set_flow_direction(barrier.flowDirection);
		gravBarrierPB.set_up_plane(barrier.upPlane);
		gravBarrierPB.set_sizex(barrier.size.x);
		gravBarrierPB.set_sizey(barrier.size.y);
		gravBarrierPB.set_activate_action((uint32_t)barrier.activateAction);
		gravBarrierPB.set_block_falling(barrier.m_blockFalling);
		
		const ECActivatable& activatable = entity.GetComponent<ECActivatable>();
		gravBarrierPB.set_name(activatable.Name());
		
		gravBarrierPB.SerializeToOstream(&stream);
	}
	
	void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
	{
		eg::Entity& entity = *ECGravityBarrier::CreateEntity(entityManager);
		
		gravity_pb::GravityBarrierEntity gravBarrierPB;
		gravBarrierPB.ParseFromIstream(&stream);
		
		entity.InitComponent<eg::ECPosition3D>(gravBarrierPB.posx(), gravBarrierPB.posy(), gravBarrierPB.posz());
		
		ECActivatable& activatable = entity.GetComponent<ECActivatable>();
		if (gravBarrierPB.name() != 0)
			activatable.SetName(gravBarrierPB.name());
		
		ECGravityBarrier& barrier = entity.GetComponent<ECGravityBarrier>();
		barrier.flowDirection = gravBarrierPB.flow_direction();
		barrier.upPlane = gravBarrierPB.up_plane();
		barrier.size = glm::vec2(gravBarrierPB.sizex(), gravBarrierPB.sizey());
		barrier.activateAction = (ECGravityBarrier::ActivateAction)gravBarrierPB.activate_action();
		barrier.m_blockFalling = gravBarrierPB.block_falling();
		
		glm::vec3 size = glm::abs(ECGravityBarrier::GetTransform(entity) * glm::vec4(0.5, 0.5, 0.1f, 0.0f));
		barrier.m_collisionShape = btBoxShape(bullet::FromGLM(size));
		
		ECRigidBody& rigidBody = entity.GetComponent<ECRigidBody>();
		rigidBody.InitStatic(barrier.m_collisionShape);
		rigidBody.GetRigidBody()->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
		rigidBody.GetRigidBody()->setActivationState(DISABLE_DEACTIVATION);
		
		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(bullet::FromGLM(entity.GetComponent<eg::ECPosition3D>().position));
		rigidBody.SetWorldTransform(transform);
	}
};

eg::IEntitySerializer* ECGravityBarrier::EntitySerializer = new GravityBarrierSerializer;
