#include "GravityBarrierEnt.hpp"
#include "../../World.hpp"
#include "../../Player.hpp"
#include "../../../Graphics/RenderSettings.hpp"
#include "../../../Graphics/Materials/MeshDrawArgs.hpp"
#include "../../../Graphics/Materials/StaticPropMaterial.hpp"
#include "../../../../Protobuf/Build/GravityBarrierEntity.pb.h"

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

GravityBarrierEnt::GravityBarrierEnt()
	: m_activatable(&GravityBarrierEnt::GetConnectionPoints), m_collisionShape({})
{
	
}

std::vector<glm::vec3> GravityBarrierEnt::GetConnectionPoints(const Ent& entity)
{
	auto [tangent, bitangent] = static_cast<const GravityBarrierEnt&>(entity).GetTangents();
	return {
		entity.Pos() + bitangent * 0.5f,
		entity.Pos() - bitangent * 0.5f,
		entity.Pos() + tangent * 0.5f,
		entity.Pos() - tangent * 0.5f
	};
}

glm::mat4 GravityBarrierEnt::GetTransform() const
{
	auto [tangent, bitangent] = GetTangents();
	return glm::mat4(
		glm::vec4(tangent, 0),
		glm::vec4(bitangent, 0),
		glm::vec4(glm::normalize(glm::cross(tangent, bitangent)), 0),
		glm::vec4(m_position, 1));
}

void GravityBarrierEnt::Draw(const EntDrawArgs& args)
{
	Draw(*args.transparentMeshBatch, *args.meshBatch);
}

void GravityBarrierEnt::EditorDraw(const EntEditorDrawArgs& args)
{
	Draw(*args.transparentMeshBatch, *args.meshBatch);
}

void GravityBarrierEnt::Draw(eg::MeshBatchOrdered& meshBatchOrdered, eg::MeshBatch& meshBatch) const
{
	auto [tangent, bitangent] = GetTangents();
	
	float tangentLen = glm::length(tangent);
	float bitangentLen = glm::length(bitangent);
	
	constexpr float TIME_SCALE = 0.25f;
	
	InstanceData instanceData;
	instanceData.position = m_position;
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

int GravityBarrierEnt::BlockedAxis() const
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

void GravityBarrierEnt::RenderSettings()
{
	Ent::RenderSettings();
	
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

const void* GravityBarrierEnt::GetComponent(const std::type_info& type) const
{
	if (type == typeid(ActivatableComp))
		return &m_activatable;
	if (type == typeid(RigidBodyComp))
		return &m_rigidBody;
	return Ent::GetComponent(type);
}

void GravityBarrierEnt::Update(const WorldUpdateArgs& args)
{
	if (args.player != nullptr)
	{
		m_enabled = true;
		int newFlowDirectionOffset = 0;
		
		if (m_activatable.m_enabledConnections != 0)
		{
			bool activated = m_activatable.AllSourcesActive();
			if (activated && activateAction == ActivateAction::Disable)
				m_enabled = false;
			else if (!activated && activateAction == ActivateAction::Enable)
				m_enabled = false;
			else if (activated && activateAction == ActivateAction::Rotate)
				newFlowDirectionOffset = 1;
		}
		
		bool decOpacity = !m_enabled;
		
		if (newFlowDirectionOffset != m_flowDirectionOffset)
		{
			if (m_opacity < 0.01f)
			{
				m_flowDirectionOffset = newFlowDirectionOffset;
			}
			else
			{
				decOpacity = true;
			}
		}
		
		constexpr float OPACITY_ANIMATION_TIME = 0.25f;
		if (decOpacity)
			m_opacity = std::max(m_opacity - args.dt / OPACITY_ANIMATION_TIME, 0.0f);
		else
			m_opacity = std::min(m_opacity + args.dt / OPACITY_ANIMATION_TIME, 1.0f);
		
		btBroadphaseProxy* broadphaseProxy = m_rigidBody.GetRigidBody()->getBroadphaseProxy();
		broadphaseProxy->m_collisionFilterMask = m_enabled ? (2 << BlockedAxis()) : 0;
		broadphaseProxy->m_collisionFilterGroup = -1;
	}
	
	BarrierBufferData bufferData;
	int itemsWritten = 0;
	
	if (args.player != nullptr)
	{
		bufferData.iaDownAxis[0] = (int)args.player->CurrentDown() / 2;
		bufferData.iaPosition[0] = glm::vec4(args.player->Position(), 0.0f);
		itemsWritten++;
	}
	
	args.world->entManager.ForEachWithFlag(EntTypeFlags::Interactable, [&](const Ent& entity)
	{
		auto* comp = entity.GetComponent<GravityBarrierInteractableComp>();
		
		if (comp != nullptr && itemsWritten < NUM_INTERACTABLES)
		{
			bufferData.iaDownAxis[itemsWritten] = (int)comp->currentDown / 2;
			bufferData.iaPosition[itemsWritten] = glm::vec4(entity.Pos(), 0.0f);
			itemsWritten++;
		}
	});
	
	while (itemsWritten < NUM_INTERACTABLES)
	{
		bufferData.iaDownAxis[itemsWritten] = 3;
		bufferData.iaPosition[itemsWritten] = glm::vec4(0.0f);
		itemsWritten++;
	}
	
	eg::DC.UpdateBuffer(material->m_barrierDataBuffer, 0, sizeof(BarrierBufferData), &bufferData);
	material->m_barrierDataBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
}

std::tuple<glm::vec3, glm::vec3> GravityBarrierEnt::GetTangents() const
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

std::pair<bool, float> GravityBarrierEnt::RayIntersect(const eg::Ray& ray) const
{
	return std::make_pair(false, 0.0f);
}

void GravityBarrierEnt::CalculateCollision(Dir currentDown, struct ClippingArgs& args) const
{
	if (m_enabled && (int)currentDown / 2 == BlockedAxis())
	{
		eg::CheckEllipsoidMeshCollision(args.collisionInfo, args.ellipsoid,
		                                args.move, barrierCollisionMesh, GetTransform());
	}
}

void GravityBarrierEnt::Spawned(bool isEditor)
{
	if (!isEditor)
	{
		m_collisionShape = btBoxShape(bullet::FromGLM(glm::abs(GetTransform() * glm::vec4(0.5, 0.5, 0.1f, 0.0f))));
		
		m_rigidBody.InitStatic(m_collisionShape);
		m_rigidBody.GetRigidBody()->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
		m_rigidBody.GetRigidBody()->setActivationState(DISABLE_DEACTIVATION);
		
		btTransform transform;
		transform.setIdentity();
		transform.setOrigin(bullet::FromGLM(m_position));
		m_rigidBody.SetWorldTransform(transform);
	}
}

void GravityBarrierEnt::Serialize(std::ostream& stream) const
{
	gravity_pb::GravityBarrierEntity gravBarrierPB;
	
	SerializePos(gravBarrierPB);
	
	gravBarrierPB.set_flow_direction(flowDirection);
	gravBarrierPB.set_up_plane(upPlane);
	gravBarrierPB.set_sizex(size.x);
	gravBarrierPB.set_sizey(size.y);
	gravBarrierPB.set_activate_action((uint32_t)activateAction);
	gravBarrierPB.set_block_falling(m_blockFalling);
	
	gravBarrierPB.set_name(m_activatable.m_name);
	
	gravBarrierPB.SerializeToOstream(&stream);
}

void GravityBarrierEnt::Deserialize(std::istream& stream)
{
	gravity_pb::GravityBarrierEntity gravBarrierPB;
	gravBarrierPB.ParseFromIstream(&stream);
	
	DeserializePos(gravBarrierPB);
	
	if (gravBarrierPB.name() != 0)
		m_activatable.m_name = gravBarrierPB.name();
	
	flowDirection = gravBarrierPB.flow_direction();
	upPlane = gravBarrierPB.up_plane();
	size = glm::vec2(gravBarrierPB.sizex(), gravBarrierPB.sizey());
	activateAction = (ActivateAction)gravBarrierPB.activate_action();
	m_blockFalling = gravBarrierPB.block_falling();
}
