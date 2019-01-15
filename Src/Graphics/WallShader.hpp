#pragma once

#include <EGame/EG.hpp>

#include "Vertex.hpp"

class WallShader
{
public:
	WallShader();
	
	void Draw(eg::BufferRef vertexBuffer, eg::BufferRef indexBuffer, uint32_t numIndices) const;
	
private:
	eg::Pipeline m_pipeline;
	eg::Texture* m_diffuseTexture;
};
