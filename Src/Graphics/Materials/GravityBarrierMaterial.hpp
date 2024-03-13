#pragma once

class GravityBarrierMaterial : public eg::IMaterial
{
public:
	static constexpr int NUM_INTERACTABLES = 8;

	struct BarrierBufferData
	{
		uint32_t iaDownAxis[NUM_INTERACTABLES];
		float iaPosition[NUM_INTERACTABLES][4];
		float gameTime;
		float _padding[3];
	};

	GravityBarrierMaterial();

	size_t PipelineHash() const override;

	bool BindPipeline(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	bool BindMaterial(eg::CommandContext& cmdCtx, void* drawArgs) const override;

	virtual OrderRequirement GetOrderRequirement() const override { return OrderRequirement::OnlyOrdered; }

	VertexInputConfiguration GetVertexInputConfiguration(const void* drawArgs) const override;

	struct Parameters
	{
		glm::vec3 position;
		glm::vec3 tangent;
		glm::vec3 bitangent;
		float opacity;
		uint32_t blockedAxis;
		float noiseSampleOffset;

		bool operator==(const Parameters&) const = default;
		bool operator!=(const Parameters&) const = default;
	};

	void SetParameters(const Parameters& parameters);

	void SetWaterDistanceTexture(eg::TextureRef waterDistanceTexture);

	static void OnInit();
	static void OnShutdown();

	static void UpdateSharedDataBuffer(const BarrierBufferData& data);

private:
	static eg::Pipeline s_pipelineGameBeforeWater;
	static eg::Pipeline s_pipelineGameFinal;
	static eg::Pipeline s_pipelineEditor;
	static eg::Pipeline s_pipelineSSR;
	static eg::Buffer s_sharedDataBuffer;

	std::optional<Parameters> m_currentAssignedParameters;

	eg::Buffer m_parametersBuffer;
	mutable eg::DescriptorSet m_descriptorSet;
	mutable bool m_hasSetWaterDistanceTexture = false;
};
