#include "GraphicsCommon.hpp"

const glm::ivec3 cubeMesh::vertices[8] =
{
	{ -1, -1, -1 },
	{ -1,  1, -1 },
	{ -1, -1,  1 },
	{ -1,  1,  1 },
	{  1, -1, -1 },
	{  1,  1, -1 },
	{  1, -1,  1 },
	{  1,  1,  1 },
};

const uint32_t cubeMesh::faces[6][4] =
{
	{ 0, 1, 2, 3 },
	{ 4, 5, 6, 7 },
	{ 0, 2, 4, 6 },
	{ 1, 3, 5, 7 },
	{ 0, 1, 4, 5 },
	{ 2, 3, 6, 7 }
};

const std::pair<uint32_t, uint32_t> cubeMesh::edges[12] =
{
	{ 0, 4 },
	{ 1, 5 },
	{ 2, 6 },
	{ 3, 7 },
	{ 0, 1 },
	{ 0, 2 },
	{ 1, 3 },
	{ 2, 3 },
	{ 4, 5 },
	{ 4, 6 },
	{ 5, 7 },
	{ 6, 7 }
};

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
