#include "GraphicsCommon.hpp"

#include <magic_enum/magic_enum.hpp>

#include "../Settings.hpp"
#include "Water/WaterRenderer.hpp"

#include "Assets/Shaders/Inc/RenderConstants.h"

bool useGLESPath;

const glm::ivec3 cubeMesh::vertices[8] = {
	{ -1, -1, -1 }, { -1, 1, -1 }, { -1, -1, 1 }, { -1, 1, 1 }, { 1, -1, -1 }, { 1, 1, -1 }, { 1, -1, 1 }, { 1, 1, 1 },
};

const uint32_t cubeMesh::faces[6][4] = {
	{ 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 0, 2, 4, 6 }, { 1, 3, 5, 7 }, { 0, 1, 4, 5 }, { 2, 3, 6, 7 },
};

const std::pair<uint32_t, uint32_t> cubeMesh::edges[12] = {
	{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }, { 0, 1 }, { 0, 2 },
	{ 1, 3 }, { 2, 3 }, { 4, 5 }, { 4, 6 }, { 5, 7 }, { 6, 7 },
};

eg::Sampler commonTextureSampler;
eg::Sampler framebufferNearestSampler;
eg::Sampler framebufferLinearSampler;
eg::Sampler linearClampToEdgeSampler;
eg::Sampler linearRepeatSampler;

eg::Texture whitePixelTexture;
eg::Texture blackPixelTexture;

void GraphicsCommonInit()
{
	const auto commonSamplerDescription = eg::SamplerDescription{ .maxAnistropy = settings.anisotropicFiltering };
	commonTextureSampler = eg::Sampler(commonSamplerDescription);

	framebufferLinearSampler = eg::Sampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::ClampToEdge,
		.wrapV = eg::WrapMode::ClampToEdge,
		.wrapW = eg::WrapMode::ClampToEdge,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Nearest,
	});

	framebufferNearestSampler = eg::Sampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::ClampToEdge,
		.wrapV = eg::WrapMode::ClampToEdge,
		.wrapW = eg::WrapMode::ClampToEdge,
		.minFilter = eg::TextureFilter::Nearest,
		.magFilter = eg::TextureFilter::Nearest,
		.mipFilter = eg::TextureFilter::Nearest,
	});
	
	linearClampToEdgeSampler = eg::Sampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::ClampToEdge,
		.wrapV = eg::WrapMode::ClampToEdge,
		.wrapW = eg::WrapMode::ClampToEdge,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Linear,
	});
	
	linearRepeatSampler = eg::Sampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::Repeat,
		.wrapV = eg::WrapMode::Repeat,
		.wrapW = eg::WrapMode::Repeat,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Linear,
	});

	eg::TextureCreateInfo whiteTextureCI;
	whiteTextureCI.format = eg::Format::R8G8B8A8_UNorm;
	whiteTextureCI.width = 1;
	whiteTextureCI.height = 1;
	whiteTextureCI.mipLevels = 1;
	whiteTextureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst;

	whiteTextureCI.label = "WhitePixel";
	whitePixelTexture = eg::Texture::Create2D(whiteTextureCI);

	whiteTextureCI.label = "BlackPixel";
	blackPixelTexture = eg::Texture::Create2D(whiteTextureCI);

	const uint8_t whitePixelTextureColor[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
	whitePixelTexture.DCUpdateData(
		eg::TextureRange{ .sizeX = 1, .sizeY = 1, .sizeZ = 1 }, sizeof(whitePixelTextureColor), whitePixelTextureColor);
	whitePixelTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	const uint8_t blackPixelTextureColor[4] = { 0, 0, 0, 0 };
	blackPixelTexture.DCUpdateData(
		eg::TextureRange{ .sizeX = 1, .sizeY = 1, .sizeZ = 1 }, sizeof(blackPixelTextureColor), blackPixelTextureColor);
	blackPixelTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

static void OnShutdown()
{
	commonTextureSampler = {};
	framebufferLinearSampler = {};
	framebufferNearestSampler = {};
	whitePixelTexture.Destroy();
	blackPixelTexture.Destroy();
}

EG_ON_SHUTDOWN(OnShutdown)

ResizableBuffer::ResizableBuffer()
{
	m_capacity = 0;
}

void ResizableBuffer::EnsureSize(uint32_t elements, uint32_t elemSize)
{
	uint64_t bytes = eg::RoundToNextMultiple((uint64_t)std::max(elements, 1U), 1024) * elemSize;
	if (bytes > m_capacity)
	{
		buffer = eg::Buffer(flags, bytes, nullptr);
		m_capacity = bytes;
	}
}

const int32_t WATER_MODE_BEFORE = 0;
const int32_t WATER_MODE_AFTER = 1;
