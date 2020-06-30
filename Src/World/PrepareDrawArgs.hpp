#pragma once

#include "../Graphics/Lighting/SpotLight.hpp"
#include "../Graphics/Lighting/PointLight.hpp"

struct PrepareDrawArgs
{
	bool isEditor;
	const class Player* player;
	std::vector<SpotLightDrawData> spotLights;
	std::vector<PointLightDrawData> pointLights;
	eg::MeshBatch* meshBatch;
	eg::MeshBatchOrdered* transparentMeshBatch;
};
