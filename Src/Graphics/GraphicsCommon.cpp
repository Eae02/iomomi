#include "GraphicsCommon.hpp"

#include <magic_enum/magic_enum.hpp>

#include "../Settings.hpp"
#include "../Water/WaterRenderer.hpp"

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

eg::SamplerHandle samplers::linearRepeatAnisotropic;
eg::SamplerHandle samplers::linearClampAnisotropic;
eg::SamplerHandle samplers::nearestClamp;
eg::SamplerHandle samplers::linearClamp;
eg::SamplerHandle samplers::linearRepeat;
eg::SamplerHandle samplers::shadowMap;

eg::DescriptorSet blackPixelTextureDescriptorSet;

eg::Buffer frameDataUniformBuffer;
char* frameDataUniformBufferPtr;
uint32_t frameDataUniformBufferOffset = 0;

constexpr size_t FRAME_DATA_UB_BYTES_PER_FRAME = 1024 * 1024;

const eg::DescriptorSetBinding FRAGMENT_SHADER_TEXTURE_BINDING = {
	.binding = 0,
	.type = eg::BindingTypeTexture{},
	.shaderAccess = eg::ShaderAccessFlags::Fragment,
};

const eg::DescriptorSetBinding FRAGMENT_SHADER_TEXTURE_BINDING_UNFILTERABLE = {
	.binding = 0,
	.type =
		eg::BindingTypeTexture{
			.sampleMode = eg::TextureSampleMode::UnfilterableFloat,
		},
	.shaderAccess = eg::ShaderAccessFlags::Fragment,
};

void GraphicsCommonInit()
{
	samplers::linearRepeatAnisotropic = eg::GetSampler({
		.wrapU = eg::WrapMode::Repeat,
		.wrapV = eg::WrapMode::Repeat,
		.wrapW = eg::WrapMode::Repeat,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Linear,
		.maxAnistropy = settings.anisotropicFiltering,
	});

	samplers::linearClampAnisotropic = eg::GetSampler({
		.wrapU = eg::WrapMode::ClampToEdge,
		.wrapV = eg::WrapMode::ClampToEdge,
		.wrapW = eg::WrapMode::ClampToEdge,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Linear,
		.maxAnistropy = settings.anisotropicFiltering,
	});

	samplers::linearClamp = eg::GetSampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::ClampToEdge,
		.wrapV = eg::WrapMode::ClampToEdge,
		.wrapW = eg::WrapMode::ClampToEdge,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Linear,
	});

	samplers::nearestClamp = eg::GetSampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::ClampToEdge,
		.wrapV = eg::WrapMode::ClampToEdge,
		.wrapW = eg::WrapMode::ClampToEdge,
		.minFilter = eg::TextureFilter::Nearest,
		.magFilter = eg::TextureFilter::Nearest,
		.mipFilter = eg::TextureFilter::Nearest,
	});

	samplers::linearRepeat = eg::GetSampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::Repeat,
		.wrapV = eg::WrapMode::Repeat,
		.wrapW = eg::WrapMode::Repeat,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Linear,
	});

	samplers::shadowMap = eg::GetSampler(eg::SamplerDescription{
		.wrapU = eg::WrapMode::Repeat,
		.wrapV = eg::WrapMode::Repeat,
		.wrapW = eg::WrapMode::Repeat,
		.minFilter = eg::TextureFilter::Linear,
		.magFilter = eg::TextureFilter::Linear,
		.mipFilter = eg::TextureFilter::Nearest,
		.enableCompare = true,
		.compareOp = eg::CompareOp::Less,
	});

	frameDataUniformBuffer = eg::Buffer(
		eg::BufferFlags::UniformBuffer | eg::BufferFlags::MapWrite | eg::BufferFlags::MapCoherent |
			eg::BufferFlags::ManualBarrier,
		FRAME_DATA_UB_BYTES_PER_FRAME * eg::MAX_CONCURRENT_FRAMES, nullptr
	);
	frameDataUniformBufferPtr = static_cast<char*>(frameDataUniformBuffer.Map());

	blackPixelTextureDescriptorSet = eg::DescriptorSet({ &FRAGMENT_SHADER_TEXTURE_BINDING_UNFILTERABLE, 1 });
	blackPixelTextureDescriptorSet.BindTexture(eg::Texture::BlackPixel(), 0);
}

static void OnShutdown()
{
	frameDataUniformBuffer.Destroy();
	blackPixelTextureDescriptorSet.Destroy();
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

uint32_t PushFrameUniformData(std::span<const char> data)
{
	uint32_t newOffset = eg::RoundToNextMultiple(
		frameDataUniformBufferOffset + data.size(), eg::GetGraphicsDeviceInfo().uniformBufferOffsetAlignment
	);
	EG_ASSERT(newOffset <= FRAME_DATA_UB_BYTES_PER_FRAME);

	uint32_t offsetWithFrameOffset = eg::CFrameIdx() * FRAME_DATA_UB_BYTES_PER_FRAME + frameDataUniformBufferOffset;

	std::memcpy(frameDataUniformBufferPtr + offsetWithFrameOffset, data.data(), data.size());

	if (!eg::HasFlag(eg::GetGraphicsDeviceInfo().features, eg::DeviceFeatureFlags::MapCoherent))
		frameDataUniformBuffer.Flush(offsetWithFrameOffset, data.size());

	frameDataUniformBufferOffset = newOffset;

	return offsetWithFrameOffset;
}

void InitFrameDataUniformBufferForNewFrame()
{
	frameDataUniformBufferOffset = 0;
}
