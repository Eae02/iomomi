#pragma once

#include <optional>

const eg::Sampler& GetCommonTextureSampler();

namespace cubeMesh
{
	extern const glm::ivec3 vertices[8];
	extern const uint32_t faces[6][4];
	extern const std::pair<uint32_t, uint32_t> edges[12];
}

class ResizableBuffer
{
public:
	eg::BufferFlags flags;
	eg::Buffer buffer;
	
	ResizableBuffer();
	
	void EnsureSize(uint32_t elements, uint32_t elemSize);
	
private:
	uint64_t m_capacity = 0;
};

enum class RenderTex
{
	GBDepth,
	GBColor1,
	GBColor2,
	Flags,
	WaterMinDepth,
	WaterMaxDepth,
	WaterTravelDepth,
	WaterDepthBlurred1,
	WaterDepthBlurred2,
	LitWithoutWater,
	Lit,
	
	MAX
};

constexpr eg::Format GB_DEPTH_FORMAT = eg::Format::Depth32;
constexpr eg::Format GB_COLOR_FORMAT = eg::Format::R8G8B8A8_UNorm;
constexpr eg::Format LIGHT_COLOR_FORMAT_LDR = eg::Format::R8G8B8A8_UNorm;
constexpr eg::Format LIGHT_COLOR_FORMAT_HDR = eg::Format::R16G16B16A16_Float;

constexpr uint32_t WATER_MODE_CONST_ID = 150;
extern const int32_t WATER_MODE_BEFORE;
extern const int32_t WATER_MODE_AFTER;

eg::Format GetFormatForRenderTexture(RenderTex texture);

//Recreates render textures if the resolution or certain graphics settings have changed.
// Called at the start of each frame.
void MaybeRecreateRenderTextures();

void RenderTextureUsageHint(RenderTex texture, eg::TextureUsage usage, eg::ShaderAccessFlags accessFlags);

//Prepares a render texture for sampling in a fragment shader
inline void RenderTextureUsageHintFS(RenderTex texture)
{
	RenderTextureUsageHint(texture, eg::TextureUsage::ShaderSample, eg::ShaderAccessFlags::Fragment);
}

eg::TextureRef GetRenderTexture(RenderTex texture);

//Gets a framebuffer made up of a set of render textures
eg::FramebufferHandle GetFramebuffer(
	std::optional<RenderTex> colorTexture1,
	std::optional<RenderTex> colorTexture2,
	std::optional<RenderTex> depthTexture,
	const char* label = nullptr);
