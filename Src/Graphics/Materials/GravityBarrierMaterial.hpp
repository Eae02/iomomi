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
#pragma pack(pop)
	
	GravityBarrierMaterial() = default;
	
	size_t PipelineHash() const override;
	
	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;
	
	glm::vec3 position;
	glm::vec3 tangent;
	glm::vec3 bitangent;
	float opacity = 0;
	int blockedAxis = 0;
	
	eg::TextureRef waterDistanceTexture;
	
	static void OnInit();
	static void OnShutdown();
	
	static void UpdateSharedDataBuffer(const BarrierBufferData& data);
	
private:
	static eg::Pipeline s_pipelineGameBeforeWater;
	static eg::Pipeline s_pipelineGameFinal;
	static eg::Pipeline s_pipelineEditor;
	static eg::Buffer s_sharedDataBuffer;
};
