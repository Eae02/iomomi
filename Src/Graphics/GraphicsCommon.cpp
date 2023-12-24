#include "GraphicsCommon.hpp"

#include <magic_enum.hpp>

#include "../Settings.hpp"
#include "Water/WaterRenderer.hpp"

#ifndef __EMSCRIPTEN__
bool useGLESPath;
#endif

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

eg::Texture whitePixelTexture;
eg::Texture blackPixelTexture;

static void OnInit()
{
	const auto commonSamplerDescription =
		eg::SamplerDescription{ .maxAnistropy = static_cast<float>(settings.anisotropicFiltering) };
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

	eg::TextureCreateInfo whiteTextureCI;
	whiteTextureCI.format = eg::Format::R8G8B8A8_UNorm;
	whiteTextureCI.width = 1;
	whiteTextureCI.height = 1;
	whiteTextureCI.mipLevels = 1;
	whiteTextureCI.flags = eg::TextureFlags::ShaderSample | eg::TextureFlags::CopyDst;
	whiteTextureCI.defaultSamplerDescription = &commonSamplerDescription;

	whiteTextureCI.label = "WhitePixel";
	whitePixelTexture = eg::Texture::Create2D(whiteTextureCI);

	whiteTextureCI.label = "BlackPixel";
	blackPixelTexture = eg::Texture::Create2D(whiteTextureCI);

	eg::DC.ClearColorTexture(whitePixelTexture, 0, eg::Color(1, 1, 1, 1));
	whitePixelTexture.UsageHint(eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);

	eg::DC.ClearColorTexture(blackPixelTexture, 0, eg::Color(0, 0, 0, 0));
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

EG_ON_INIT(OnInit)
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
