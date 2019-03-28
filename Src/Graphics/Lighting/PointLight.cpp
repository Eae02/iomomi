#include "PointLight.hpp"

PointLightDrawData PointLight::GetDrawData(const glm::vec3& position) const
{
	PointLightDrawData data;
	data.pc.position = position;
	data.pc.radiance = Radiance();
	data.pc.range = Range();
	data.instanceID = InstanceID();
	return data;
}
