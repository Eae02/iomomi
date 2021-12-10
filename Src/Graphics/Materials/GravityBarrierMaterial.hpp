#pragma once

class GravityBarrierMaterial : public eg::IMaterial
{
public:
	static constexpr int NUM_INTERACTABLES = 8;
	
	struct __attribute__ ((__packed__, __may_alias__)) BarrierBufferData
	{
		uint32_t iaDownAxis[NUM_INTERACTABLES];
		float iaPosition[NUM_INTERACTABLES][4];
		float gameTime;
		float _padding[3];
	};
	
	GravityBarrierMaterial() = default;
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyOrdered; }
	
	glm::vec3 position;
	glm::vec3 tangent;
	glm::vec3 bitangent;
	float opacity = 0;
	int blockedAxis = 0;
	float noiseSampleOffset = 0;
	
	eg::TextureRef waterDistanceTexture;
	
	static void OnInit();
	static void OnShutdown();
	
	static void UpdateSharedDataBuffer(const BarrierBufferData& data);
	
private:
	static eg::Pipeline s_pipelineGameBeforeWater;
	static eg::Pipeline s_pipelineGameFinal;
	static eg::Pipeline s_pipelineEditor;
	static eg::Pipeline s_pipelineSSR;
	static eg::Buffer s_sharedDataBuffer;
};
