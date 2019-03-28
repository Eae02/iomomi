#pragma once

#include "../../Graphics/Lighting/PointLight.hpp"

class IPointLightEntity
{
public:
	virtual void GetPointLights(std::vector<PointLightDrawData>& drawData) const = 0;
};
