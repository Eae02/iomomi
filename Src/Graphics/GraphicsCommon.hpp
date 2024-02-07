#pragma once

#include <optional>

extern eg::Sampler commonTextureSampler;
extern eg::Sampler framebufferNearestSampler;
extern eg::Sampler framebufferLinearSampler;
extern eg::Sampler linearClampToEdgeSampler;
extern eg::Sampler linearRepeatSampler;

extern eg::Texture whitePixelTexture;
extern eg::Texture blackPixelTexture;
extern eg::Texture defaultShadowMap;

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

constexpr uint32_t WATER_MODE_CONST_ID = 150;

constexpr uint32_t COMMON_3D_DEPTH_OFFSET_CONST_ID = 151;

extern const int32_t WATER_MODE_BEFORE;
extern const int32_t WATER_MODE_AFTER;

extern const float ZNear;
extern const float ZFar;
