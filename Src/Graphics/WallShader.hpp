#pragma once

#include "Vertex.hpp"

struct WallMaterial
{
	bool initialized = false;
	const char* name;
	float textureScale = 1;
	float minRoughness = 1;
	float maxRoughness = 1;
};

constexpr size_t MAX_WALL_MATERIALS = 9;
extern WallMaterial wallMaterials[MAX_WALL_MATERIALS];

void InitializeWallShader();

void BindWallShaderGame();
void BindWallShaderEditor(bool drawGrid);
void BindWallShaderPointLightShadow(const struct PointLightShadowDrawArgs& renderArgs);

void DrawWallBordersEditor(eg::BufferRef vertexBuffer, uint32_t numVertices);
