#pragma once

#include "../World/Entities/EntTypes/GravityBarrierEnt.hpp"

class WaterBarrierRenderer
{
public:
	WaterBarrierRenderer();

	void Init(const class WaterSimulator2* simulator, class World& world);

	void Update(float dt);

	static constexpr eg::Format DEPTH_TEXTURE_FORMAT = eg::Format::Depth16;

private:
	eg::Pipeline m_barrierPipeline;

	eg::Texture m_defaultTexture;

	eg::DescriptorSet m_descriptorSet;

	struct Barrier
	{
		std::shared_ptr<GravityBarrierEnt> entity;
		eg::Texture texture;
		eg::Framebuffer framebuffer;
		glm::mat4 transform;
	};

	std::vector<Barrier> m_barriers;

	uint32_t m_numParticles;
};
