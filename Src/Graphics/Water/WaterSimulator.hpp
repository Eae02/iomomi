#pragma once

#include <memory>
#include <future>
#include <thread>
#include <optional>
#include <mutex>

#include "WaterSimulatorImpl.hpp"
#include "WaterPumpDescription.hpp"
#include "../../World/Dir.hpp"

class WaterSimulator
{
public:
	static constexpr float GAME_TIME_OFFSET = 100;
	
	struct QueryResults
	{
		int numIntersecting;
		glm::vec3 waterVelocity;
		glm::vec3 buoyancy;
	};
	
	class QueryAABB
	{
	public:
		friend class WaterSimulator;
		
		void SetAABB(const eg::AABB& aabb)
		{
			m_aabbMT = aabb;
		}
		
		QueryResults GetResults()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_results;
		}
		
	private:
		std::mutex m_mutex;
		QueryResults m_results;
		
		eg::AABB m_aabbMT;
		eg::AABB m_aabbBT;
	};
	
	WaterSimulator() = default;
	
	~WaterSimulator()
	{
		Stop();
	}
	
	void Init(class World& world);
	
	void Stop();
	
	void Update(const World& world, const glm::vec3& cameraPos, bool paused);
	
	//Finds an intersection between the given ray and the water.
	// Returns a pair of distance and particle position.
	// distance will be infinity if no intersection was found.
	std::pair<float, glm::vec3> RayIntersect(const eg::Ray& ray) const;
	
	//Changes gravitation for all particles close to and connected to the given position.
	void ChangeGravity(const glm::vec3& particlePos, Dir newGravity, bool highlightOnly = false)
	{
		m_changeGravityParticleMT = ChangeGravityParticleRef { particlePos, newGravity, highlightOnly };
	}
	
	eg::BufferRef GetPositionsBuffer() const { return m_positionsBuffer; }
	eg::BufferRef GetGravitiesBuffer() const { return m_gravitiesBuffer; }
	
	uint32_t NumParticles() const
	{
		return m_numParticles;
	}
	
	uint32_t NumParticlesToDraw() const
	{
		return m_numParticlesToDraw;
	}
	
	void AddQueryAABB(std::weak_ptr<QueryAABB> queryAABB)
	{
		m_queryAABBs.push_back(std::move(queryAABB));
	}
	
	//Returns the number of nanoseconds for the last update
	uint64_t LastUpdateTime() const
	{
		return m_lastUpdateTime;
	}
	
	bool IsPresimComplete();
	
	bool needParticleGravityBuffer = false;
	
private:
	void ThreadTarget();
	
	std::mutex m_mutex;
	bool m_run = false;
	
	uint32_t m_presimIterationsCompleted = 0;
	uint32_t m_targetPresimIterations = 0;
	
	uint32_t m_lastGravityBufferVersion = 0;
	
	std::atomic_uint64_t m_lastUpdateTime { 0 };
	
	std::vector<std::weak_ptr<QueryAABB>> m_queryAABBs;
	std::vector<std::shared_ptr<QueryAABB>> m_queryAABBsBT;
	
	uint32_t m_numParticles = 0;
	uint32_t m_numParticlesToDraw = 0;
	
	struct ChangeGravityParticleRef
	{
		glm::vec3 particlePos;
		Dir newGravity;
		bool highlightOnly;
	};
	
	std::optional<ChangeGravityParticleRef> m_changeGravityParticleMT;
	std::vector<ChangeGravityParticleRef> m_changeGravityParticles;
	
	std::vector<WaterBlocker> m_waterBlockersMT;
	std::vector<WaterBlocker> m_waterBlockersSH;
	
	std::vector<WaterPumpDescription> m_waterPumpsMT;
	std::vector<WaterPumpDescription> m_waterPumpsSH;
	
	float m_gameTimeSH = 0;
	
	bool m_pausedSH = false;
	std::condition_variable m_unpausedSignal;
	
	struct WaterSimulatorImpl* m_impl;
	
	eg::Buffer m_positionsUploadBuffer;
	char* m_positionsUploadBufferMemory;
	eg::Buffer m_positionsBuffer;
	
	eg::Buffer m_gravitiesUploadBuffer;
	char* m_gravitiesUploadBufferMemory;
	eg::Buffer m_gravitiesBuffer;
	
	volatile const float* m_currentParticlePositions = nullptr;
	
	std::thread m_thread;
};
