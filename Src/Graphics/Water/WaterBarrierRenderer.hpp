#pragma once

#include "../../World/Entities/EntTypes/GravityBarrierEnt.hpp"

class WaterBarrierRenderer
{
public:
	WaterBarrierRenderer();
	
	void Init(class IWaterSimulator* waterSimulator, class World& world);
	
	void Update(float dt);
	
private:
#ifdef IOMOMI_ENABLE_WATER
	eg::Pipeline m_calcPipeline;
	eg::Pipeline m_fadePipeline;
	
	eg::Texture m_defaultTexture;
	
	struct Barrier
	{
		std::shared_ptr<GravityBarrierEnt> entity;
		eg::Texture tmpTexture;
		eg::Texture fadeTexture;
		glm::mat4 transform;
		eg::DescriptorSet descriptorSetCalc;
		eg::DescriptorSet descriptorSetFade;
	};
	
	std::vector<Barrier> m_barriers;
	
	uint32_t m_numParticles;
	uint32_t m_dispatchCount;
#endif
};
