#pragma once

struct WaterQueryResults
{
	int numIntersecting = 0;
	glm::vec3 waterVelocity;
	glm::vec3 buoyancy;
};

class WaterQueryAABB
{
public:
	friend class WaterSimulationThread;

	explicit WaterQueryAABB(const eg::AABB& aabb = {}) : m_aabb(aabb) {}

	void SetAABB(const eg::AABB& aabb)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_aabb = aabb;
	}

	WaterQueryResults GetResults()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_results;
	}

private:
	std::mutex m_mutex;
	WaterQueryResults m_results;

	eg::AABB m_aabb;
};
