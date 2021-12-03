#ifdef IOMOMI_NO_WATER
#include "WaterSimulator.hpp"

void WaterSimulator::Init(class World& world) { }
void WaterSimulator::Stop() { }
bool WaterSimulator::IsPresimComplete() { return true; }
void WaterSimulator::Update(const class World& world, const glm::vec3& cameraPos, bool paused) { }

std::pair<float, glm::vec3> WaterSimulator::RayIntersect(const eg::Ray& ray) const
{
	return { INFINITY, glm::vec3(0) };
}

#endif
