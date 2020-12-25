#pragma once

#include "../Graphics/Lighting/PointLight.hpp"

struct PrepareDrawArgs
{
	bool isEditor;
	const class Player* player;
	eg::MeshBatch* meshBatch;
	eg::MeshBatchOrdered* transparentMeshBatch;
	eg::Frustum* frustum;
};
