#pragma once

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
