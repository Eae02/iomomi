#pragma once

class WaterSimulator
{
public:
	WaterSimulator();
	
	void Init(const class World& world);
	
	void Update(float dt);
	
	eg::BufferRef GetPositionsBuffer() const;
	
	uint32_t NumParticles() const;
	
private:
	uint32_t m_localSizeX;
	uint32_t m_dispatchSizeX;
	
	uint32_t m_numParticles;
	
	eg::Pipeline m_pipeline0DetectClose;
	eg::Pipeline m_pipeline1DensityComp;
	eg::Pipeline m_pipeline2AccelComp;
	eg::Pipeline m_pipeline3VelDiffusion;
	eg::Pipeline m_pipeline4Collision;
	
	eg::Buffer m_positionsBuffer;
	eg::Buffer m_densityBuffer;
	eg::Buffer m_velocityBuffers[2];
	int m_mainVelocityBuffer = 0;
	int m_auxVelocityBuffer = 1;
	
	eg::Buffer m_numCloseBuffer;
	eg::Texture m_closeParticlesImage;
	
	eg::Buffer m_noiseBuffer;
	
	eg::Texture m_voxelSolidityImage;
	glm::ivec3 m_solidityMin;
};
