#pragma once

class GravityBarrierMaterial : public eg::IMaterial
{
public:
	static constexpr int NUM_INTERACTABLES = 8;
	
#pragma pack(push, 1)
	struct BarrierBufferData
	{
		uint32_t iaDownAxis[NUM_INTERACTABLES];
		glm::vec4 iaPosition[NUM_INTERACTABLES];
		float gameTime;
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
	
	GravityBarrierMaterial();
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	eg::Buffer m_barrierDataBuffer;
	
private:
	eg::Pipeline m_pipelineGameBeforeWater;
	eg::Pipeline m_pipelineGameAfterWater;
	eg::DescriptorSet m_descriptorSetGame;
	
	eg::Pipeline m_pipelineEditor;
	eg::DescriptorSet m_descriptorSetEditor;
};
