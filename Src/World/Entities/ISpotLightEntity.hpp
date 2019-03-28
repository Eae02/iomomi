#pragma once

#include "../../Graphics/Lighting/SpotLight.hpp"

class ISpotLightEntity
{
public:
	virtual void GetSpotLights(std::vector<SpotLightDrawData>& drawData) const = 0;
};
