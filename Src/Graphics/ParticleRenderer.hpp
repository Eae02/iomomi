#pragma once

class ParticleRenderer
{
public:
	ParticleRenderer();

	void Draw(const eg::ParticleManager& particleManager, eg::TextureRef depthTexture);

	const eg::Texture& GetTexture() const { return *m_texture; }

private:
	eg::Pipeline m_pipeline;
	eg::Buffer m_vertexBuffer;
	const eg::Texture* m_texture;
};
