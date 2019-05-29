#include "PointLight.hpp"

PointLightDrawData PointLight::GetDrawData(const glm::vec3& position) const
{
	PointLightDrawData data;
	data.pc.position = position;
	data.pc.radiance = Radiance();
	data.pc.range = Range();
	data.pc.invRange = 1.0f / data.pc.range;
	data.instanceID = InstanceID();
	data.castsShadows = castsShadows;
	return data;
}
