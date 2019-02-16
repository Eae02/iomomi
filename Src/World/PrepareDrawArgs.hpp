#pragma once

#include "../Graphics/Lighting/SpotLight.hpp"
#include "../Graphics/ObjectRenderer.hpp"

struct PrepareDrawArgs
{
	bool isEditor;
	std::vector<SpotLightDrawData> spotLights;
	ObjectRenderer* objectRenderer;
};
