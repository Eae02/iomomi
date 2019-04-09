#pragma once

#include "../Graphics/Lighting/SpotLight.hpp"
#include "../Graphics/Lighting/PointLight.hpp"

struct PrepareDrawArgs
{
	bool isEditor;
	std::vector<SpotLightDrawData> spotLights;
	std::vector<PointLightDrawData> pointLights;
	std::vector<eg::Plane> reflectionPlanes;
	eg::MeshBatch* meshBatch;
};
