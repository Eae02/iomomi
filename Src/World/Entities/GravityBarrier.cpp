#include "GravityBarrier.hpp"
#include "ECEditorVisible.hpp"
#include "Messages.hpp"
#include "ECRigidBody.hpp"
#include "ECActivatable.hpp"
#include "../BulletPhysics.hpp"
#include "../Clipping.hpp"
#include "../../Graphics/Materials/MeshDrawArgs.hpp"
#include "../../Graphics/RenderSettings.hpp"
#include "../../../Protobuf/Build/GravityBarrierEntity.pb.h"

#include <imgui.h>

static constexpr float OPACITY_SCALE = 0.0004;

struct InstanceData
{
	glm::vec3 position;
	float opacity;
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
		pipelineCI.vertexAttributes[1] = { 1, eg::DataType::Float32, 4, offsetof(InstanceData, position) };
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
}

static void OnShutdown()
{
	delete material;
	vertexBuffer.Destroy();
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

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
		position - bitangent * 0.5f
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

void ECGravityBarrier::Draw(eg::Entity& entity, eg::MeshBatchOrdered& meshBatch) const
{
	auto [tangent, bitangent] = GetTangents();
	
	constexpr float TIME_SCALE = 0.25f;
	
	InstanceData instanceData;
	instanceData.position = entity.GetComponent<eg::ECPosition3D>().position;
	instanceData.opacity = OPACITY_SCALE * glm::smoothstep(0.0f, 1.0f, m_opacity);
	instanceData.tangent = tangent;
	instanceData.bitangent = bitangent;
	instanceData.tangentMag = glm::length(tangent);
	instanceData.bitangentMag = glm::length(bitangent) * TIME_SCALE;
	
	meshBatch.Add(barrierMesh, *material, instanceData, DepthDrawOrder(instanceData.position));
}

int ECGravityBarrier::BlockedAxis() const
{
	auto [tangent, bitangent] = GetTangents();
	if (std::abs(tangent.x) > 0.5f)
		return 0;
	if (std::abs(tangent.y) > 0.5f)
		return 1;
	return 2;
}

void ECGravityBarrier::Update(eg::EntityManager& entityManager, float dt)
{
	for (eg::Entity& entity : entityManager.GetEntitySet(EntitySignature))
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
			barrier.m_opacity = std::max(barrier.m_opacity - dt / OPACITY_ANIMATION_TIME, 0.0f);
		else
			barrier.m_opacity = std::min(barrier.m_opacity + dt / OPACITY_ANIMATION_TIME, 1.0f);
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
	
	ImGui::Combo("Plane", &upPlane, "X\0Y\0Z\0");
	
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
		gravBarrierPB.set_flowdirection(barrier.flowDirection);
		gravBarrierPB.set_upplane(barrier.upPlane);
		gravBarrierPB.set_sizex(barrier.size.x);
		gravBarrierPB.set_sizey(barrier.size.y);
		gravBarrierPB.set_activateaction((uint32_t)barrier.activateAction);
		
		const ECActivatable& activatable = entity.GetComponent<ECActivatable>();
		gravBarrierPB.set_name(activatable.Name());
		gravBarrierPB.set_reqactivations(activatable.EnabledConnections());
		
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
		activatable.SetEnabledConnections(gravBarrierPB.reqactivations());
		
		ECGravityBarrier& barrier = entity.GetComponent<ECGravityBarrier>();
		barrier.flowDirection = gravBarrierPB.flowdirection();
		barrier.upPlane = gravBarrierPB.upplane();
		barrier.size = glm::vec2(gravBarrierPB.sizex(), gravBarrierPB.sizey());
		barrier.activateAction = (ECGravityBarrier::ActivateAction)gravBarrierPB.activateaction();
		
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
