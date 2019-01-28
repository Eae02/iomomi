#pragma once

#include "Vertex.hpp"

void InitializeWallShader();

void DrawWalls(const std::function<void()>& drawCallback);

void DrawWallsEditor(const std::function<void()>& drawCallback);

void DrawWallBordersEditor(eg::BufferRef vertexBuffer, uint32_t numVertices);
