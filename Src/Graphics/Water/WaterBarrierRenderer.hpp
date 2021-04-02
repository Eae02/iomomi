#pragma once

#include "../../World/Entities/EntTypes/GravityBarrierEnt.hpp"

class WaterBarrierRenderer
{
public:
	WaterBarrierRenderer();
	
	void Init(class WaterSimulator& waterSimulator, class World& world);
	
	void Update();
	
private:
	eg::Pipeline m_pipeline;
	
	eg::Texture m_defaultTexture;
	
	struct Barrier
	{
		std::shared_ptr<GravityBarrierEnt> entity;
		eg::Texture texture;
		glm::mat4 transform;
		eg::DescriptorSet descriptorSet;
	};
	
	std::vector<Barrier> m_barriers;
	
	uint32_t m_numParticles;
	uint32_t m_dispatchCount;
};
