#pragma once

#include "GraphicsCommon.hpp"

struct PhysicsDebugVertex
{
	float x;
	float y;
	float z;
	uint32_t color;
};

struct PhysicsDebugRenderData
{
	std::vector<PhysicsDebugVertex> vertices;
	std::vector<uint32_t> triangleIndices;
	std::vector<uint32_t> lineIndices;
};

class PhysicsDebugRenderer
{
public:
	PhysicsDebugRenderer();

	void Prepare(const PhysicsDebugRenderData& renderData, const glm::mat4& viewProjTransform);
	void Prepare(const class PhysicsEngine& physicsEngine, const glm::mat4& viewProjTransform);

	void Render();

private:
	PhysicsDebugRenderData m_renderData;

	ResizableBuffer m_vertexBuffer;
	ResizableBuffer m_indexBuffer;

	eg::Pipeline m_trianglePipeline;
	eg::Pipeline m_linesPipeline;

	uint32_t m_numLineIndices = 0;
	uint32_t m_numTriangleIndices = 0;

	eg::Buffer m_viewProjBuffer;
	eg::DescriptorSet m_descriptorSet;
};
