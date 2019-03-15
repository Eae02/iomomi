#pragma once

#include "../Graphics/Lighting/SpotLight.hpp"

struct PrepareDrawArgs
{
	bool isEditor;
	std::vector<SpotLightDrawData> spotLights;
	eg::MeshBatch* meshBatch;
};
