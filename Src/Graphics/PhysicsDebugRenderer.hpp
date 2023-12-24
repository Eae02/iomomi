#pragma once

#include "GraphicsCommon.hpp"

struct __attribute__((__packed__, __may_alias__)) PhysicsDebugVertex
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

	void Render(
		const class PhysicsEngine& physicsEngine, const glm::mat4& viewProjTransform, eg::TextureRef colorFBTex,
		eg::TextureRef depthFBTex);
	void Render(
		const PhysicsDebugRenderData& renderData, const glm::mat4& viewProjTransform, eg::TextureRef colorFBTex,
		eg::TextureRef depthFBTex);

private:
	PhysicsDebugRenderData m_renderData;

	ResizableBuffer m_vertexBuffer;
	ResizableBuffer m_indexBuffer;

	eg::Pipeline m_trianglePipeline;
	eg::Pipeline m_linesPipeline;

	eg::TextureRef m_prevColorFBTex;
	eg::TextureRef m_prevDepthFBTex;
	eg::Framebuffer m_framebuffer;
};
