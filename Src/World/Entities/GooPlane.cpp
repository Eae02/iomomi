#include "GooPlane.hpp"
#include "ECWallMounted.hpp"
#include "ECLiquidPlane.hpp"
#include "Messages.hpp"
#include "../../Graphics/PlanarReflectionsManager.hpp"
#include "../../Graphics/RenderSettings.hpp"
#include "../../Graphics/Materials/MeshDrawArgs.hpp"
#include "../../../Protobuf/Build/GooPlaneEntity.pb.h"

namespace GooPlane
{
	constexpr int NM_SAMPLES = 3;
	constexpr float NM_SCALE_GLOBAL = 3.5f;
	constexpr float NM_SPEED_GLOBAL = 0.15f;
	constexpr float NM_ANGLES[NM_SAMPLES] = { 0.25f * eg::TWO_PI, 0.5f * eg::TWO_PI, 0.75f * eg::TWO_PI };
	constexpr float NM_SCALES[NM_SAMPLES] = { 1.2f, 1.0f, 0.8f };
	constexpr float NM_SPEED[NM_SAMPLES] = { 0.75f, 1.0f, 1.25f };
	
	struct GooPlaneMaterial : eg::IMaterial
	{
		size_t PipelineHash() const override
		{
			return typeid(GooPlaneMaterial).hash_code();
		}
		
		bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override
		{
			MeshDrawMode drawMode = static_cast<MeshDrawArgs*>(drawArgs)->drawMode;
			if (!(drawMode == MeshDrawMode::Game || drawMode == MeshDrawMode::Emissive))
				return false;
			
			cmdCtx.BindPipeline(drawMode == MeshDrawMode::Game ? s_pipeline : s_emissivePipeline);
			
			cmdCtx.BindDescriptorSet(s_descriptorSet, 0);
			
			return true;
		}
		
		bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override
		{
			if (static_cast<MeshDrawArgs*>(drawArgs)->drawMode == MeshDrawMode::Game)
			{
				cmdCtx.BindTexture(m_reflectionPlane.texture, 1, 0);
			}
			
			return true;
		}
		
		ReflectionPlane m_reflectionPlane;
		
		static eg::Pipeline s_pipeline;
		static eg::Pipeline s_emissivePipeline;
		static eg::Buffer s_textureTransformsBuffer;
		static eg::DescriptorSet s_descriptorSet;
	};
	
	eg::Pipeline GooPlaneMaterial::s_pipeline;
	eg::Pipeline GooPlaneMaterial::s_emissivePipeline;
	eg::Buffer GooPlaneMaterial::s_textureTransformsBuffer;
	eg::DescriptorSet GooPlaneMaterial::s_descriptorSet;
	
	static glm::vec3 waterColor;
	
	static void OnInit()
	{
		eg::GraphicsPipelineCreateInfo pipelineCI;
		pipelineCI.vertexShader = eg::GetAsset<eg::ShaderModule>("Shaders/GooPlane.vs.glsl").Handle();
		pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/GooPlane.fs.glsl").Handle();
		pipelineCI.enableDepthWrite = true;
		pipelineCI.enableDepthTest = true;
		pipelineCI.cullMode = eg::CullMode::Back;
		pipelineCI.frontFaceCCW = true;
		pipelineCI.numColorAttachments = 2;
		pipelineCI.setBindModes[0] = eg::BindMode::DescriptorSet;
		pipelineCI.setBindModes[1] = eg::BindMode::Dynamic;
		pipelineCI.vertexBindings[0] = { sizeof(glm::vec3), eg::InputRate::Vertex };
		pipelineCI.vertexAttributes[0] = { 0, eg::DataType::Float32, 3, 0 };
		GooPlaneMaterial::s_pipeline = eg::Pipeline::Create(pipelineCI);
		
		pipelineCI.fragmentShader = eg::GetAsset<eg::ShaderModule>("Shaders/GooPlane-Emissive.fs.glsl").Handle();
		pipelineCI.numColorAttachments = 1;
		pipelineCI.depthCompare = eg::CompareOp::LessOrEqual;
		pipelineCI.enableDepthWrite = false;
		GooPlaneMaterial::s_emissivePipeline = eg::Pipeline::Create(pipelineCI);
		
		GooPlaneMaterial::s_descriptorSet = eg::DescriptorSet(GooPlaneMaterial::s_pipeline, 0);
		
		struct
		{
			glm::vec4 color;
			glm::vec4 nmTransforms[NM_SAMPLES * 2];
		} bufferData;
		
		waterColor = glm::vec3(0.1f, 0.9f, 0.2f);
		bufferData.color = glm::vec4(waterColor, 0.3f);
		
		for (int i = 0; i < NM_SAMPLES; i++)
		{
			const float sinT = std::sin(NM_ANGLES[i]);
			const float cosT = std::cos(NM_ANGLES[i]);
			const float scale = 1.0f / (NM_SCALES[i] * NM_SCALE_GLOBAL);
			bufferData.nmTransforms[i * 2 + 0] = glm::vec4(cosT, 0, -sinT, NM_SPEED[i] * NM_SPEED_GLOBAL) * scale;
			bufferData.nmTransforms[i * 2 + 1] = glm::vec4(sinT, 0,  cosT, NM_SPEED[i] * NM_SPEED_GLOBAL) * scale;
		}
		GooPlaneMaterial::s_textureTransformsBuffer = eg::Buffer(eg::BufferFlags::UniformBuffer, sizeof(bufferData), &bufferData);
		GooPlaneMaterial::s_textureTransformsBuffer.UsageHint(eg::BufferUsage::UniformBuffer, eg::ShaderAccessFlags::Fragment);
		
		GooPlaneMaterial::s_descriptorSet.BindUniformBuffer(RenderSettings::instance->Buffer(), 0, 0, RenderSettings::BUFFER_SIZE);
		GooPlaneMaterial::s_descriptorSet.BindUniformBuffer(GooPlaneMaterial::s_textureTransformsBuffer, 1, 0, sizeof(bufferData));
		GooPlaneMaterial::s_descriptorSet.BindTexture(eg::GetAsset<eg::Texture>("Textures/SlimeN.png"), 2);
	}
	
	static void OnShutdown()
	{
		GooPlaneMaterial::s_pipeline = { };
		GooPlaneMaterial::s_descriptorSet = { };
		GooPlaneMaterial::s_textureTransformsBuffer = { };
	}
	
	EG_ON_INIT(OnInit)
	EG_ON_SHUTDOWN(OnShutdown)
	
	struct ECGooPlane
	{
		void HandleMessage(eg::Entity& entity, const DrawMessage& message);
		
		static eg::MessageReceiver MessageReceiver;
		
		GooPlaneMaterial m_material;
	};
	
	eg::MessageReceiver ECGooPlane::MessageReceiver = eg::MessageReceiver::Create<ECGooPlane, DrawMessage>();
	
	eg::EntitySignature EntitySignature = eg::EntitySignature::Create<
	    eg::ECPosition3D, ECWallMounted, ECLiquidPlane, ECEditorVisible, ECGooPlane>();
	
	void ECGooPlane::HandleMessage(eg::Entity& entity, const DrawMessage& message)
	{
		ECLiquidPlane& liquidPlane = entity.GetComponent<ECLiquidPlane>();
		liquidPlane.MaybeUpdate(entity, *message.world);
		
		m_material.m_reflectionPlane.plane = { glm::vec3(0, 1, 0), entity.GetComponent<eg::ECPosition3D>().position };
		
		if (liquidPlane.NumIndices() != 0)
		{
			message.reflectionPlanes->push_back(&m_material.m_reflectionPlane);
			message.meshBatch->Add(liquidPlane.GetMesh(), m_material);
		}
	}
	
	eg::Entity* CreateEntity(eg::EntityManager& entityManager)
	{
		eg::Entity& entity = entityManager.AddEntity(EntitySignature, nullptr, EntitySerializer);
		
		entity.InitComponent<ECEditorVisible>("Goo");
		
		ECLiquidPlane& liquidPlane = entity.GetComponent<ECLiquidPlane>();
		liquidPlane.SetShouldGenerateMesh(true);
		liquidPlane.SetEditorColor(eg::ColorSRGB::FromRGBAHex(0x619F4996));
		
		return &entity;
	}
	
	struct Serializer : eg::IEntitySerializer
	{
		std::string_view GetName() const override
		{
			return "GooPlane";
		}
		
		void Serialize(const eg::Entity& entity, std::ostream& stream) const override
		{
			gravity_pb::GooPlaneEntity gooPlanePB;
			
			glm::vec3 pos = entity.GetComponent<eg::ECPosition3D>().position;
			gooPlanePB.set_posx(pos.x);
			gooPlanePB.set_posy(pos.y);
			gooPlanePB.set_posz(pos.z);
			
			gooPlanePB.SerializeToOstream(&stream);
		}
		
		void Deserialize(eg::EntityManager& entityManager, std::istream& stream) const override
		{
			eg::Entity& entity = *CreateEntity(entityManager);
			
			gravity_pb::GooPlaneEntity gooPlanePB;
			gooPlanePB.ParseFromIstream(&stream);
			
			entity.InitComponent<eg::ECPosition3D>(gooPlanePB.posx(), gooPlanePB.posy(), gooPlanePB.posz());
		}
	};
	
	eg::IEntitySerializer* EntitySerializer = new Serializer;
}
