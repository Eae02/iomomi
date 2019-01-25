#pragma once

#include "Vertex.hpp"

void DrawWalls(const class RenderSettings& renderSettings, eg::BufferRef vertexBuffer,
               eg::BufferRef indexBuffer, uint32_t numIndices);
