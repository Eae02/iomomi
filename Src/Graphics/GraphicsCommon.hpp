#pragma once

#include <optional>
#include <span>

namespace samplers
{
extern eg::SamplerHandle linearRepeatAnisotropic;
extern eg::SamplerHandle linearClampAnisotropic;
extern eg::SamplerHandle nearestClamp;
extern eg::SamplerHandle linearClamp;
extern eg::SamplerHandle linearRepeat;
extern eg::SamplerHandle shadowMap;
}; // namespace samplers

extern eg::Texture defaultShadowMap;

extern eg::DescriptorSet blackPixelTextureDescriptorSet;

void GraphicsCommonInit();

extern bool useGLESPath;

namespace cubeMesh
{
extern const glm::ivec3 vertices[8];
extern const uint32_t faces[6][4];
extern const std::pair<uint32_t, uint32_t> edges[12];
} // namespace cubeMesh

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

extern const eg::DescriptorSetBinding FRAGMENT_SHADER_TEXTURE_BINDING;
extern const eg::DescriptorSetBinding FRAGMENT_SHADER_TEXTURE_BINDING_UNFILTERABLE;

extern eg::Buffer frameDataUniformBuffer;

uint32_t PushFrameUniformData(std::span<const char> data);

void InitFrameDataUniformBufferForNewFrame();

constexpr uint32_t WATER_MODE_CONST_ID = 150;

constexpr uint32_t COMMON_3D_DEPTH_OFFSET_CONST_ID = 151;

extern const int32_t WATER_MODE_BEFORE;
extern const int32_t WATER_MODE_AFTER;

extern const float ZNear;
extern const float ZFar;
