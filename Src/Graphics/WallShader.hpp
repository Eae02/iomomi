#pragma once

#include "Vertex.hpp"

void InitializeWallShader();

void BindWallShaderGame();
void BindWallShaderEditor();
void BindWallShaderPointLightShadow(const struct PointLightShadowRenderArgs& renderArgs);
void BindWallShaderPlanarReflections(const eg::Plane& plane);

void DrawWallBordersEditor(eg::BufferRef vertexBuffer, uint32_t numVertices);
