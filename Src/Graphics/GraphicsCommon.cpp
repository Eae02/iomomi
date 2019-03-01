#include "GraphicsCommon.hpp"

static eg::Sampler commonTextureSampler;

static void OnInit()
{
	eg::SamplerDescription samplerDescription;
	samplerDescription.maxAnistropy = 16;
	commonTextureSampler = eg::Sampler(samplerDescription);
}

static void OnShutdown()
{
	commonTextureSampler = { };
}

EG_ON_INIT(OnInit)
EG_ON_SHUTDOWN(OnShutdown)

const eg::Sampler& GetCommonTextureSampler()
{
	return commonTextureSampler;
}
