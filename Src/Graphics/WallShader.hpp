#pragma once

#include "Vertex.hpp"

void InitializeWallShader();

void DrawWalls(eg::BufferRef vertexBuffer, eg::BufferRef indexBuffer, uint32_t numIndices);

void DrawWallsEditor(eg::BufferRef vertexBuffer, eg::BufferRef indexBuffer, uint32_t numIndices);
