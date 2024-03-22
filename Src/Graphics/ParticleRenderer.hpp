#pragma once

class ParticleRenderer
{
public:
	ParticleRenderer();

	void SetDepthTexture(eg::TextureRef depthTexture);

	void Draw(const eg::ParticleManager& particleManager);

private:
	eg::DescriptorSet m_descriptorSet0;
	eg::DescriptorSet m_descriptorSet1;
};
