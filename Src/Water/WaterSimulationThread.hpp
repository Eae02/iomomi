#pragma once

#include "../World/Dir.hpp"
#include "WaterQuery.hpp"

class WaterSimulationThread
{
public:
	struct GravityChange
	{
		eg::Ray ray;
		float maxDistance;
		Dir newGravity;
	};

	static constexpr uint32_t FRAME_CYCLE_LENGTH = eg::MAX_CONCURRENT_FRAMES + 1;

	WaterSimulationThread(glm::vec3 gridOrigin, uint32_t numParticles);

	~WaterSimulationThread();

	WaterSimulationThread(WaterSimulationThread&&) = delete;
	WaterSimulationThread(const WaterSimulationThread&) = delete;
	WaterSimulationThread& operator=(WaterSimulationThread&&) = delete;
	WaterSimulationThread& operator=(const WaterSimulationThread&) = delete;

	void PushGravityChangeMT(const GravityChange& gravityChange);

	void AddQueryMT(std::weak_ptr<WaterQueryAABB> query);

	void OnFrameBeginMT(std::span<const glm::uvec2> particleData);

	struct GravityChangeResult
	{
		Dir newGravity;
		std::unique_ptr<uint32_t[]> changeGravityBits;
	};

	std::optional<GravityChangeResult> PopGravityChangeResult();

private:
	void ThreadTarget();

	glm::vec3 m_gridOrigin;
	uint32_t m_numParticles = 0;

	std::mutex m_mutex;

	std::vector<std::weak_ptr<WaterQueryAABB>> m_newWaterQueries;

	std::vector<GravityChange> m_gravityChangesMT;
	std::vector<GravityChange> m_gravityChangesBT;

	std::vector<GravityChangeResult> m_gravityChangeResults;

	std::condition_variable m_signalForBackThread;

	std::unique_ptr<glm::uvec2[]> m_particleDataMT;
	std::unique_ptr<glm::uvec2[]> m_particleDataBT;

	bool m_workAvailable = false;
	bool m_shouldStop = false;

	std::thread m_thread;
};
